-- Copyright 2012-2014 Christopher E. Miller
-- License: GPLv3, see LICENSE file.

require("utils")
require("internal")
require("timersl")
require("sockets")
require("ircprotocol")


--[[
local function hook(e)
	local d = debug.getinfo(2, "Sn")
	io.stderr:write((e or "") .. " " .. (d.source or "?") .. "(" .. (d.linedefined or "?") .. "): " .. (d.namewhat or "?") .. " " .. (d.name or "?") .. "\n")
	io.stderr:flush()
end
-- debug.sethook(hook, "c") -- trace
debug.sethook(hook, "cr") -- trace
--]]


-- -interactive
-- -input:RUN=$RUN {1+}

irccmd = true
addresses_set = addresses_set or nil
port_set = port_set or nil
nick_set = nick_set or nil -- can contain %x special sequences to be translated.
alt_nick_set = alt_nick_set or nil -- use IrcClient:nick() to get the real nick.
password_set = password_set or nil
doraw = doraw or nil
interactive = interactive or nil
loadscripts = loadscripts or nil
ircclients = ircclients or {} -- currently connected IRC clients
manager = manager or SelectManager()
testmode = testmode or nil
noreconnect = noreconnect or nil


clientAdded = event()
clientRemoved = event()
exiting = event()


bold = string.char(2)
color = string.char(3)
underline = string.char(0x1F)
reverse = string.char(0x16)
normal = string.char(0x0F)


-- How console input is handled.
-- The key is the command the user types in such as /MSG
-- The value is what is sent to the server.
comm_input = comm_input or {
	MSG = "PRIVMSG {1} :{2+}",
	NOTICE = "NOTICE {1} :{2+}",
	NICK = "NICK {1}",
	JOIN = "JOIN {1}",
	PART = "PART {1} {2...}",
	MODE = "MODE {1} {2} {3...}",
	-- QUIT = "QUIT {1...}",
	QUIT = "$RUN client:sendLine([[QUIT :{1...}]]); noreconnect=true;",
	TOPIC = "TOPIC {1} {2...}",
	KICK = "KICK {1} {2} {3...}",
	INVITE = "INVITE {1} {2}",
	RAW = "{1}",
	DIE = "$RUN client:sendLine('QUIT'); client:disconnect(); manager:stop('all'); noreconnect=true;",
	ECHO = "$ECHO {1...}",
	["$DEFAULT"] = "PRIVMSG {1} :{2+}",
}

-- From server to display, :prefix (optional source) and KEY command name are assumed.
-- Values are assumed to be $ECHO but others can be used (e.g. $RUN)
comm_output = comm_output or {
	PRIVMSG = "{target} <{nick}> {msg}",
	-- PRIVMSG = "$RUN echo('{target} <{nick}> {msg} = #' .. string.len('{msg}'))", -- warning, injection!
	NOTICE = "{target} *{nick}* {msg}",
	NICK = " *** {nick} is now known as {newnick}",
	JOIN = " *** {nick} has joined {target}",
	PART = " *** {nick} has parted {target} ({msg...})",
	MODE = " *** {nick} sets mode {modes} {params...} for {target}",
	QUIT = " *** {nick} has quit ({msg...})",
	TOPIC = " *** {nick} changed topic of {target} to '{topic}'",
	KICK = " *** {nick} has kicked {kicked} from {target} ({msg...})",
	INVITE = " *** {invited} has been invited to {target}",
	["353"] = " - {names}", -- NAMES info
	["366"] = " - ", -- end of NAMES
	-- ["433"] = " *** {newnick}: {msg}", -- nick already in use
	["?"] = " *** {msg...}", -- any command not specified in this table.
}


function randomDigit()
	return string.char(internal.frandom(string.byte("0"), string.byte("9") + 1))
end

function randomUpperLetter()
	return string.char(internal.frandom(string.byte("A"), string.byte("Z") + 1))
end

function randomLowerLetter()
	return string.char(internal.frandom(string.byte("a"), string.byte("z") + 1))
end


function replacements2(s)
	return string.gsub(s, "%%(.?)", function(rep)
			if rep == 'd' then
				return randomDigit()
			elseif rep == 'A' then
				return randomUpperLetter()
			elseif rep == 'a' then
				return randomLowerLetter()
			elseif rep == 'n' then
				local x = internal.frandom(2, 3 + 1)
				local s = ""
				for i = 1, x do
					s = s .. randomDigit()
				end
				return s
			elseif rep == 'S' then
				local x = internal.frandom(2, 3 + 1)
				local s = ""
				for i = 1, x do
					s = s .. randomUpperLetter()
				end
				return s
			elseif rep == 's' then
				local x = internal.frandom(2, 3 + 1)
				local s = ""
				for i = 1, x do
					s = s .. randomLowerLetter()
				end
				return s
			else
				error("Unsupported escape %" .. rep)
			end
		end)
end


-- argv[1] is app name
function do_cmdline(argc, argv)
	local ordarg = 2
	for iarg = ordarg, argc do
		local arg = argv[iarg]
		if string.sub(arg, 1, 1) == '-' then
			local argvalue = ""
			local ieq = arg:find("=", 1, true)
			if ieq then
				argvalue = arg:sub(ieq + 1)
				arg = arg:sub(1, ieq - 1)
			end
			ordarg = 60000; -- Disable ord args after a -switch.
			if arg == "-address" or arg == "-addr"
				or arg == "-addresses" or arg == "-addrs" then
				addresses_set = argvalue
			elseif arg == "-port" then
				port_set = tonumber(argvalue)
			elseif arg == "-nick" then
				nick_set = argvalue
			elseif arg == "-altnick" or arg == "-alt_nick" then
				alt_nick_set = argvalue
			elseif arg == "-password" or arg == "-pass" then
				password_set = argvalue
			elseif arg == "-raw" then
				if argvalue == "" or argvalue:lower() == "stderr" then
					doraw = io.stderr
				elseif argvalue:lower() == "stdout" then
					doraw = io.stdout
				else
					local f, err = io.open(argvalue, "a+")
					if not f then
						error("Problem with -raw: " .. err)
					end
					doraw = f
				end
			elseif arg == "-test" then
				testmode = true
			elseif arg == "-interactive" then
				interactive = true
			elseif arg == "-load" then
				if not loadscripts then loadscripts = {} end
				table.insert(loadscripts, argvalue)
			elseif arg == "-flag" then
				_G[argvalue] = true
			elseif arg == "-input_clear" then
				comm_input = {}
			elseif arg == "-output_clear" then
				comm_output = {}
			elseif arg == "-noreconnect" then
				noreconnect = true
			elseif arg:sub(1, 7) == "-input:" then
				local ic = arg:sub(7 + 1):upper()
				if ic == "MSG" then ic = "PRIVMSG" end
				comm_input[ic] = argvalue
			elseif arg:sub(1, 8) == "-output:" then
				local ic = arg:sub(8 + 1):upper()
				if ic == "MSG" then ic = "PRIVMSG" end
				comm_output[ic] = argvalue
			else
				error("Unknown switch: " .. arg)
			end
		else
			if ordarg == 2 then addresses_set = arg
			elseif ordarg == 3 then nick_set = arg
			elseif ordarg == 4 then alt_nick_set = arg
			else error("Unexpected: " .. arg) end
			ordarg = ordarg + 1
		end
	end
end


IrcCmdClient = class(IrcClient)

function IrcCmdClient:init()
	IrcClient.init(self)
end

function IrcCmdClient:connect(...)
	if select(1, ...) then
		self._dest = {...}
		return IrcClient.connect(self, ...)
	elseif self._dest then
		return IrcClient.connect(self, unpack(self._dest))
	end
	return nil, "No connect destination"
end

function IrcCmdClient:onConnected()
	local nick, alt_nick = replacements2(self.nick_set), replacements2(self.alt_nick_set)
	if self.password_set then
		self:sendLine("PASS " .. self.password_set)
	end
	-- self:sendLine("NICK " .. nick)
	self:nick(nick)
	self:sendLine("USER " .. nick .. " b c :" .. nick)
	clientAdded(self)
end

function IrcCmdClient:onDisconnected(msg, code)
	manager:remove(self)
	self:destroy()
	clientRemoved(self)
	if not noreconnect then
		-- Reconnect in N secs.
		local client = self
		Timer(45, function(tmr)
			tmr:stop()
			if not client:valid() then
				if client:connect() then -- reuse old args
					manager:add(self)
				end
			end
		end):start()
	end
	IrcClient.onDisconnected(self, msg, code)
end


function combinefail(...)
	if not select(1, ...) then
		local s = select(2, ...)
		local added = false
		for i = 3, select('#', ...) do
			if i == 3 then
				added = true
				s = s .. " ("
			else
				s = s .. ", "
			end
			s = s .. tostring(select(i, ...))
		end
		if added then
			s = s .. ")"
		end
		return select(1, ...), s
	end
	return ...
end


function addIrcClient(settings)
	if type(settings) == "string" then
		settings = { addresses = settings }
	end
	if not settings.addresses then
		-- addrs = "irc.freenode.net"
		error("Address expected")
	end
	if not settings.nick then
		settings.nick = nick_set or "Guest%d%d%d%d"
		if not settings.alt_nick then
			settings.alt_nick = alt_nick_set or settings.nick
		end
	end
	if not settings.alt_nick then
		settings.alt_nick = settings.nick .. "_%d"
	end
	if not settings.port then
		settings.port = port_set or 6667
	end
	
	-- print(" nick = " .. nick .. " - alt_nick = " .. alt_nick .. " ")
	-- print(" connecting to " .. addrs .. " ")

	local addr
	for xa in settings.addresses:gmatch("[^,; ]+") do
		addr = xa
	end
	
	local client = IrcCmdClient()
	for k, v in pairs(settings) do
		client[k .. "_set"] = v
	end
	if testmode then
		io.stderr:write("Test mode, not connecting to IRC\n")
		client:onReceiveLine(":server 001 Test :Welcome")
		client:onReceiveLine(":server 005 Test :")
	else
		-- assert(client:connect(addr, port))
		-- assert(client:connect(addr, port, "STREAM", "UNSPEC"))
		-- For IPv6 set host to: IPv6+host
		local addr6 = addr:match("^[iI][pP][vV]6%+(.*)$")
		if addr6 then
			io.stderr:write("Connecting to '", addr6, "' (IPv6)...\n")
			assert(combinefail(client:connect(addr6, settings.port, "STREAM", "INET6")))
		else
			io.stderr:write("Connecting to '", addr, "'...\n")
			assert(combinefail(client:connect(addr, settings.port, "STREAM", "INET")))
		end
	end
	-- internal.console_print("Connected!\n");
	io.stderr:write("Connected!\n")

	client:blocking(false) -- set nonblocking
	client:linger(2)

	table.insert(ircclients, client)
	-- clientAdded(client)

	-- Setup event handlers for output (e.g. PRIVMSG, NOTICE)
	for serverCmd, serverCmdSyntax in pairs(IrcClient.readServerCommands) do
		if serverCmdSyntax and serverCmdSyntax:len() then
			local infoSyntax = comm_output[serverCmd]
			if infoSyntax then
				-- print("client.on['" .. serverCmd .. "'] handler setup")
				client.on[serverCmd] = function(client, prefix, cmd, params)
					client:doDefaultServerAction(client, prefix, cmd, params)
				end
			end
		end
	end

	enableSendLineTimer(client, 1.2, 80, 4)
	
	if not testmode then
		manager:add(client)
	end
	
	return client
end


-- argv[1] is app name
function irccmd_startup(argc, argv)
	do_cmdline(argc, argv)
	
	local xt, xtmsg = xpcall(function() --------------------
	
	local client = addIrcClient{
		nick = nick_set,
		alt_nick = alt_nick_set,
		addresses = addresses_set,
		port = port_set,
		password = password_set,
	}

	if loadscripts then
		for i = 1, #loadscripts do
			local lf, xerr = loadfile(loadscripts[i])
			if not lf then
				io.stderr:write("Unable to load script ", loadscripts[i], ": ", xerr or "", "\n")
			else
				local xok, xerr = xpcall(lf, debug.traceback)
				if not xok then
					io.stderr:write("Error running script ", loadscripts[i], ": ", xerr or "", "\n")
				end
			end
		end
	end

	local connected = true
	local needprompt = interactive
	local numerrors = 0

	Timer(1, function()
		-- Heal timer:
		if numerrors > 0 then
			numerrors = numerrors - 1
		end
	end):start()

	manager.onStandardInput = function(_, input)
		local ln = input:gsub("[\r\n]+", "")
		if ln:sub(1, 1) == "/" then
			local cmd, strparams = ln:match("/([^ ]+)[ ]?(.*)")
			client:doUserCommand(comm_input, cmd, strparams)
		elseif ln ~= "" then
			client:doUserCommand(comm_input, "$DEFAULT", ln)
		end
	end

	while connected do -- while connected
		local xt2, xt2err = xpcall(function() -- xpcall
			while connected do
				if not manager:loop() then
					connected = false
					break
				end
			end
		end, -- xpcall
		debug.traceback)
		if xt2 then
			break
		else
			numerrors = numerrors + 5
			if numerrors >= 100 then
				connected = false
				error(xt2err)
			else
				io.stderr:write("MAIN ERROR: ", xt2err, "\n")
			end
		end
	end  -- while connected

	exiting()

	disableSendLineTimer(client)

	manager:remove(client)

	client:destroy()

	-- internal.console_print("Done!\n");

	end,  --------------------
	debug.traceback)
	if not xt then
		error("FATAL ERROR: " .. xtmsg)
	end

	if doraw then
		doraw:close()
	end

end


-- Do this last so that everything else is setup.
pcall(require, "ircnicklist") -- Not a hard dependency.


if internal._icDebug then
	-- print("package.path=", package.path)
	-- print("package.cpath=", package.cpath)
	local testargs = { "debugapp", "localhost", "Foo%n%S%s", "-load=gamearmleg.lua", "-raw" }
	irccmd_startup(#testargs, testargs) -- Run a test when debugging.
end

