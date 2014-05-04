irccmd
======

Short for IRC Command, a scriptable command-line IRC client.

Depends on
<a href="http://www.lua.org/versions.html#5.1">Lua 5.1</a>.

* Install Lua 5.1 using your platform's favorite package manager, or using the download from the Lua site. The packages may be named similarly to lua5.1 and liblua5.1-0-dev.

Fetch irccmd. To build:
```
cd src
make LUA_INCLUDE=/usr/include/lua5.1
cd ..
```
If the above cannot find your lua include headers, change LUA_INCLUDE=... based on where the dev headers are on your system.

Run it:
```
LUA_PATH=lua/?.lua ./irccmd <server>
```
Switches supported:
```
-nick=<value> - set your primary nickname.
-altnick=<value> - set alternate nickname in case primary is taken.
-interactive - is this session interactive? tries to preserve lines.
-noreconnect - don't reconnect automatically upon disconnection.
-load=<file.lua> - load a lua source file as a custom script/plugin/bot.
-input:<cmd>=<syntax> - input cmd, such as "-input:RUN=$RUN {1+}" creates /run.
-output:<cmd>=<syntax> - output cmd.
-flag=<global> - set a global variable to true, useful with loaded scripts.
```
