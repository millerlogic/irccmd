# When running, irccmd switches are to be passed in the irccmd_args env var:
#   docker run -it --rm=true -e irccmd_args="irc.server.gom -nick=User22 -noreconnect" millerlogic/irccmd

FROM debian:7

RUN apt-get update

# For: downloading.
RUN apt-get install wget -y

# Compiler and tools.
RUN apt-get install gcc make -y

# Lua compile deps.
RUN apt-get install libreadline6-dev -y
RUN apt-get install ncurses-dev -y # libncurses5-dev

# Get lua source.
RUN wget -O/tmp/lua-5.1.5.tar.gz http://www.lua.org/ftp/lua-5.1.5.tar.gz
RUN cd /tmp && tar xzf lua-5.1.5.tar.gz

# Add the host dir.
ADD ./ /irccmd

# Update lua source.
RUN cp -f /irccmd/src/lua-5.1/* /tmp/lua-5.1.5/src

# Get lua dev headers so we can build modules.
RUN apt-get install liblua5.1-0-dev -y

# Build and install lua.
RUN cd /tmp/lua-5.1.5/ && make linux && make install

# Get and build BitOp module.
RUN wget -O/tmp/LuaBitOp-1.0.2.tar.gz http://bitop.luajit.org/download/LuaBitOp-1.0.2.tar.gz
RUN cd /tmp && tar xzf LuaBitOp-1.0.2.tar.gz
RUN cd /tmp/LuaBitOp-1.0.2 && make INCLUDES=-I/usr/include/lua5.1 && make install

# Build irccmd.
RUN cd /irccmd && cc -o irccmd src/*.c $(pkg-config lua5.1 --cflags --libs)

CMD cd /irccmd && LUA_PATH=/irccmd/lua/?.lua ./irccmd $irccmd_args
