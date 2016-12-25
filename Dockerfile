# When running, irccmd switches are to be passed in the IRCCMD_ARGS env var:
#   docker run -it --rm=true -e IRCCMD_ARGS="irc.server.gom -nick=User22 -noreconnect" millerlogic/irccmd

FROM debian:7

RUN apt-get update

# For: downloading.
RUN apt-get install wget -y

# Compiler and tools.
RUN apt-get install gcc make -y

# Patch.
RUN apt-get install patch -y

# Lua compile deps.
RUN apt-get install libreadline6-dev -y
RUN apt-get install ncurses-dev -y # libncurses5-dev

# Get lua source.
RUN wget -O/tmp/lua-5.1.5.tar.gz http://www.lua.org/ftp/lua-5.1.5.tar.gz
RUN cd /tmp && tar xzf lua-5.1.5.tar.gz

# Add the host dir.
ADD ./ /irccmd

# Patch with coco coroutines.
RUN cd /tmp/lua-5.1.5 && patch -f -p1 </irccmd/src/patches/lua-5.1.5-coco-1.1.7.patch

# Update lua source.
RUN cp -f /irccmd/src/lua-5.1/* /tmp/lua-5.1.5/src

#RUN apt-get install liblua5.1-0-dev -y

# Build and install lua.
RUN cd /tmp/lua-5.1.5/ && make linux && make install

# Get luarocks
RUN apt-get install git -y
RUN apt-get install luarocks -y

# Get some stuff from luarocks
RUN luarocks install luabitop
RUN luarocks install luaposix

# Build irccmd.
RUN cd /irccmd && cc -shared -fPIC -o irccmd_internal.so src/*.c \
    -I/usr/include/lua5.1 -lm -ldl

RUN groupadd -g 28101 container || echo
RUN useradd -u 28101 -N -g 28101 container || echo

RUN mkdir /irccmd-state
RUN chown container:container /irccmd-state

VOLUME ["/irccmd-state"]

USER container
CMD cd /irccmd && LUA_PATH=/irccmd/lua/?.lua ./irccmd $IRCCMD_ARGS
