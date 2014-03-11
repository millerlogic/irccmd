-- Copyright 2012-2014 Christopher E. Miller
-- License: GPLv3, see LICENSE file.

require("internal")
require("utils")


function nickFromSource(source)
	local ibang = source:find("!", 1, true)
	if ibang then
		return source:sub(1, ibang - 1)
	end
	return source
end
assert(nickFromSource("hello!world@addr.com") == "hello")
assert(nickFromSource("server.name") == "server.name")


-- Returns: "user@host" from the format "nick!user@host", or returns nil.
function addressFromSource(source)
	local ibang = source:find("!", 1, true)
	if ibang then
		return source:sub(ibang + 1)
	end
	return nil
end
assert(addressFromSource("hello!world@addr.com") == "world@addr.com")
assert(addressFromSource("server.name") == nil)

-- Returns: "host" from the format "nick!user@host" or "server.name", or returns nil.
function siteFromSource(source)
	local iat = source:find("@", 1, true)
	if iat then
		return source:sub(iat + 1)
	end
	return nil
end
assert(siteFromSource("hello!world@addr.com") == "addr.com")
assert(siteFromSource("server.name") == nil)

-- Returns 1 to 3 values of: (nick, user, host)
function sourceParts(source)
	local a, b, c = source:match("^([^!]+)!?([^@]*)@?(.*)$")
	-- if not a or a:len() == 0 then a = nil end
	if not b or b:len() == 0 then b = nil end
	if not c or c:len() == 0 then c = nil end
	return a, b, c
end
local testA, testB, testC = sourceParts("hello!world@addr.com")
assert(testA == "hello" and testB == "world" and testC == "addr.com")
local testA, testB, testC = sourceParts("server.name")
assert(testA == "server.name" and testB == nil and testC == nil)

-- Returns: "user" from the format "nick!user@host", or returns nil.
function userNameFromSource(source)
	return select(2, sourceParts(source))
end
assert(userNameFromSource("hello!world@addr.com") == "world")
assert(userNameFromSource("server.name") == nil)


IrcClient = class(SocketClientLines)

function IrcClient:init(socket)
	SocketClientLines.init(self, socket)
	-- print("Initializing IrcClient")

	self.strcmp = internal.compare_rfc1459

	-- self.support = {}
	-- self.prefixSymbols = ""
	-- self.prefixModes = ""

	self.on = {}
	local onmt = {}
	onmt.__newindex = function(t, key, value)
		-- print("event", key, "from", debug.getinfo(1).source)
		-- Alter the key so that __newindex is always called.
		key = "/" .. key
		local e = rawget(t, key)
		if not e then
			e = event()
			rawset(t, key, e)
		end
		e:add(value)
	end
	onmt.__index = function(t, key)
		key = "/" .. key
		return rawget(t, key)
	end
	setmetatable(self.on, onmt)

	self.on["PING"] = function(client, prefix, cmd, params)
		if #params >= 1 then
			self:sendLine("PONG :" .. params[1])
		else
			self:sendLine("PONG")
		end
		-- internal.console_print("Ping? Pong!\n")
	end
end

-- Returns: channel name, or nil if not a channel. e.g. returns "#Lua" from "@+#Lua"
-- non-nil return is guaranteed to start with a channel prefix.
function IrcClient:channelNameFromTarget(target)
	for i = 1, target:len() do
		-- Skip prefix symbols.
		if not self.prefixSymbols:find(target:sub(i, i), 1, true) then
			-- Found non-prefix symbol, now make sure it's a channel prefix.
			if self.support["CHANTYPES"]:find(target:sub(i, i), 1, true) then
				return target:sub(i)
			end
			break
		end
	end
	return nil
end

-- Determines if the string is a channel name.
-- Note: channel mode prefixes are not considered, use self:channelNameFromTarget in that case.
function IrcClient:isChannelName(chan)
	if self.support["CHANTYPES"]:find(chan:sub(1, 1), 1, true) then
		return true
	end
	return false
end

-- Uses the current server's case mapping to compare case insensitive strings.
-- Returns: 0 on match, < 0 if less, or > 0 if greater.
-- Note: no self parameter.
function IrcClient.strcmp(s1, s2)
	return internal.compare_rfc1459(s1, s2)
end

IrcClient.readServerCommands = {
	PRIVMSG = "{target} {msg}",
	NOTICE = "{target} {msg}",
	NICK = "{newnick}",
	JOIN = "{target}",
	PART = "{target} {msg}",
	MODE = "{target} {modes} {params...}",
	QUIT = "{msg}",
	TOPIC = "{target} {topic}",
	KICK = "{chan} {kicked} {msg}",
	INVITE = "{invited} {chan}",
	["353"] = "{target} {symbol} {chan} {names}", -- NAMES info
	["366"] = "{target} {symbol} {msg}", -- end of NAMES
	["433"] = "{target} {newnick} {msg}", -- nick already in use
	["?"] = "{target} {msg...}", -- any command not specified in this table.
}

--[[
IrcClient.writeServerCommands = {
	PRIVMSG = "PRIVMSG {target} {msg}",
	NOTICE = "NOTICE {target} {msg}",
	NICK = "NICK {newnick}",
	JOIN = "JOIN {target}",
	PART = "PART {target} {msg}",
	MODE = "MODE {target} {modes} {params...}",
	QUIT = "QUIT {msg}",
	TOPIC = "TOPIC {target} {topic}",
	KICK = "KICK {chan} {kicked} {msg}",
	INVITE = "INVITE {invited} {chan}",
}
--]]

function IrcClient:doDefaultServerAction(client, prefix, serverCmd, params)
	-- print("Default action for " .. serverCmd)
	local serverCmdSyntax = IrcClient.readServerCommands[serverCmd]
	if not serverCmdSyntax then
		serverCmdSyntax = IrcClient.readServerCommands["?"]
	end
	if serverCmdSyntax and serverCmdSyntax:len() then
		local infoSyntax = comm_output[serverCmd]
		if not infoSyntax then
			infoSyntax = comm_output["?"]
		end
		if infoSyntax and infoSyntax:len() then
			local values = {}
			values["source"] = prefix
			values["cmd"] = serverCmd
			getServerCommandValues(serverCmdSyntax, params, values)
			if infoSyntax:sub(1, 1) == '$' then
				local magicCmd, magicInfoSyntax = infoSyntax:match("([^ ]+)[ ]?(.*)")
				local info = self:translate2(magicInfoSyntax, values)
				doMagic(self, magicCmd, info)
			else
				local info = self:translate2(infoSyntax, values)
				-- doMagic(self, "$ECHO", info)
				internal.console_print(info, "\n")
			end
		end
	end
end

-- IRC newline characters will be appended.
function IrcClient:sendLine(line)
	if doraw then
		doraw:write("SEND: `", line, "`\n")
	end
	-- assert(not line:find("\n", 1, true), "Did not expect newline characters in IrcClient:sendLine(line)")
	return self:send(line .. "\r\n")
end

-- Returns the channel user mode for the prefix, or nil if not a prefix on this server.
function IrcClient:prefixToMode(prefix)
	assert(prefix:len() == 1)
	local pos = self.prefixSymbols:find(prefix)
	if pos then
		return self.prefixModes:sub(pos, pos)
	end
	return nil
end

-- Returns the prefix for the channel user mode, or nil if not a prefix mode on this server.
function IrcClient:modeToPrefix(mode)
	assert(mode:len() == 1)
	local pos = self.prefixModes:find(mode)
	if pos then
		return self.prefixSymbols:sub(pos, pos)
	end
	return nil
end

-- Returns: channel user prefix and nick. e.g. returns ("+", "JoeUser") from "+JoeUser"
function IrcClient:getNickInfo(nickEntry)
	for i = 1, nickEntry:len() do
		-- Skip channel user prefix symbols.
		if not self.prefixSymbols:find(nickEntry:sub(i, i), 1, true) then
			-- Found non-prefix symbol.
			if i == 1 then return "", nickEntry end
			return nickEntry:sub(1, i - 1), nickEntry:sub(i)
		end
	end
	return nickEntry, ""
end

-- Returns: plain nick without channel user prefix. e.g. returns "JoeUser" from "+JoeUser"
function IrcClient:getPlainNick(nickEntry)
	local prefix, nick = self:getNickInfo(nickEntry)
	return nick
end

--  newnick is optional
function IrcClient:nick(newnick)
	if newnick then
		self._nick = newnick
		self:sendLine("NICK " .. newnick)
	end
	return self._nick
end

function IrcClient:network()
	-- TODO: use server if no NETWORK
	return self.support["NETWORK"] or "Unknown"
end

-- cmd will always be uppercase.
-- params is a table with the parameters.
function IrcClient:onCommand(prefix, cmd, params)
	-- internal.console_print("got command ", cmd, " with params #", #params, "\n")
	-- print("IRC:", prefix or "<nil>", cmd or "<nil>", unpack(params)) -----

	-- Only observe commands here, don't respond to any of them.
	-- If any commands need response, add to event handler.

	if cmd == "001" or (cmd == "NICK" and nickFromSource(prefix or "") == self._nick) then
		self._nick = params[1]
		-- internal.console_print("Nick set to ", self.nick(), "\n")
	end

	if cmd == "001" then
		self.strcmp = internal.compare_rfc1459
		self.support = {}
		-- Default ISUPPORT values:
		self.support["CASEMAPPING"] = "rfc1459"
		self.support["CHANNELLEN"] = 200
		self.support["CHANTYPES"] = "#&"
		self.support["MODES"] = 3
		self.support["NICKLEN"] = 9
		self.support["TARGMAX"] = ""
		self.support["PREFIX"] = "(ov)@+"
		self.prefixModes = "ov"
		self.prefixSymbols = "@+"
		assert(self:prefixToMode("+") == "v")
		assert(self:prefixToMode(">") == nil)
		assert(self:prefixToMode("@") == "o")
		assert(self:modeToPrefix("v") == "+")
		assert(self:modeToPrefix("s") == nil)
		assert(self:modeToPrefix("o") == "@")
		assert(self:getPlainNick("+JoeUser") == "JoeUser")
		assert(self:getPlainNick("JoeUser") == "JoeUser")
		assert(self:getPlainNick("@JoeUser") == "JoeUser")
	elseif cmd == "005" then
		for i = 2, #params do
			local k = params[i]
			if k ~= "are supported by this server" then
				local ieq = k:find("=", 1, true)
				local v = ""
				if ieq then
					v = k:sub(ieq + 1)
					k = k:sub(1, ieq - 1)
				end
				if v:match("^[%-+]?%d+$") then
					v = tonumber(v, 10)
				elseif k == "PREFIX" then
					local pmodes, psyms = v:match("^%(([^%(%)]+)%)([^%(%)]+)$")
					if pmodes and psyms and pmodes:len() == psyms:len() then
						self.prefixModes = pmodes
						self.prefixSymbols = psyms
					end
				elseif k == "CASEMAPPING" then
					if v == "ascii" then
						self.strcmp = internal.compare_ascii
					elseif v == "rfc1459" then
						self.strcmp = internal.compare_rfc1459
					elseif v == "strict-rfc1459" then
						self.strcmp = internal.compare_strict_rfc1459
					else
						io.stderr:write("WARNING: unknown CASEMAPPING: ", v, "\n")
						self.strcmp = internal.compare_rfc1459
					end
				end
				-- print("", "ISUPPORT", k, type(v), v) -----
				self.support[k] = v
			end
		end
	end

	if self.on["*"] then
		if "stop" == self.on["*"](self, prefix, cmd, params) then return "stop" end
	end
	if self.on[cmd] then
		if "stop" == self.on[cmd](self,prefix, cmd, params) then return "stop" end
	elseif self.on["?"] then
		if "stop" == self.on["?"](self,prefix, cmd, params) then return "stop" end
	end
end

function IrcClient:onReceiveLine(line)
	line = internal.irc_input(line) -- fix the IRC line
	-- internal.console_print("IRC: ", line, "\n"); -----
	if doraw then
		doraw:write("READ: ", line, "\n")
	end
	local prefix, cmd, params = internal.irc_parse(line)
	if cmd then
		cmd = cmd:upper()
		return self:onCommand(prefix, cmd, params)
	else
		io.stderr:write("WARNING: invalid command received: ", line, "\n")
	end
end

function IrcClient:sendMsg(to, msg, priority)
	self:sendLine("PRIVMSG " .. to .. " :" .. msg, priority)
end

function IrcClient:sendNotice(to, msg, priority)
	self:sendLine("NOTICE " .. to .. " :" .. msg, priority)
end

function IrcClient:sendCommand(cmd, ...)
	local line = cmd
	local n = select('#', ...)
	for i = 1, n do
		local s = select(i, ...)
		if s:find(' ', 1, true) then
			assert(i == n, "Only the last parameter can contain spaces")
			line = line .. " :" .. s
		else
			line = line .. " " .. s
		end
	end
	self:sendLine(line)
end


function IrcClient:translate2(pattern, values)
	return pattern:gsub("%{([^%}]*)%}", function(rawname)
		local hasdots = "..." == rawname:sub(-3)
		local name
		if hasdots then
			name = rawname:sub(0, -4)
		else
			name = rawname
		end
		if name == "nick" then
			if values["nick"] then
				return values["nick"]
			elseif values["source"] then
				return nickFromSource(values["source"])
			end
		elseif name == "target" then
			if values["target"] then
				return values["target"];
			end
		elseif name == "chan" or name == "channel" then
			if values["channel"] then
				return values["channel"]
			elseif values["chan"] then
				return values["chan"]
			elseif values["target"] then
				return self:channelNameFromTarget(values["target"]);
			end
		elseif name == "msg" or name == "message" then
			if values["msg"] then
				return values["msg"]
			elseif values["message"] then
				return values["message"]
			end
		elseif values[name] then
			return values[name]
		end
		if hasdots then
			return ""
		end
		return "{" .. name .. "}"
	end)
end


function getServerCommandValues(commandSyntax, params, values)
	values = values or {}
	local paramindex = 1
	for rawname in commandSyntax:gmatch("%{([^%}]*)%}") do
		local hasdots = "..." == rawname:sub(-3)
		local name
		if hasdots then
			name = rawname:sub(0, -4)
		else
			name = rawname
		end
		-- print("*", name, "=", params[paramindex])
		values[name] = params[paramindex]
		paramindex = paramindex + 1
		if hasdots then
			while paramindex <= #params do
				values[name] = values[name] .." " .. params[paramindex]
				paramindex = paramindex + 1
			end
		end
	end
	return values
end


function getClientParameterValues(commandSyntax, strparams, values)
	values = values or {}
	local lastnum = 0
	for rawname in commandSyntax:gmatch("%{([^%}]*)%}") do
		local num, extra = rawname:match("^(%d+)([%+]?[%.]?[%.]?[%.]?)$")
		if num then
			local hasplus
			local hasdots
			if extra == "+" then
				hasplus = true
			elseif extra == "..." then
				hasdots = true
			elseif extra ~= "" then
				num = nil
			end
			if num then
				if tonumber(num) ~= lastnum + 1 then
					strparams = ""
				end
				if hasplus then
					values[rawname] = strparams
					strparams = ""
				elseif hasdots then
					values[num] = strparams
					strparams = ""
				else
					local x, y = strparams:match("^([^ ]*) ?(.*)")
					values[num] = x
					strparams = y
				end
				lastnum = num
			end
		end
	end
	return values
end


local function _runecho(...)
	internal.console_print(..., "\n")
end


function doMagic(client, magicCmd, magicInfo)
	magicCmd = magicCmd:upper()
	if magicCmd == "$RUN" then
		local run = "return function(client, values, echo) \t " .. magicInfo .. " \t end"
		local fn, xerr = loadstring(run, "RUN_" .. os.time())
		if not fn then
			io.stderr:write(" $ Error with command: ", magicCmd, ": ", xerr or "", "\n")
		else
			local fnrun = fn()
			local xok, xerr = pcall(fnrun, client, values, _runecho)
			if not xok then
				io.stderr:write(" $ Error with command: ", magicCmd, ": ", xerr or "", "\n")
			end
		end
	elseif magicCmd == "$ECHO" then
		internal.console_print(magicInfo, "\n")
	else
		-- io.stderr:write(" $ Unknown command: ", magicCmd, "\n")
		client:sendLine(client:translate2(syntax, values))
	end
end


function IrcClient:doUserCommand(syntax_table, cmd, strparams)
	if cmd and cmd:len() > 0 then
		cmd = cmd:upper()
		local syntax = syntax_table[cmd]
		if syntax then
			if syntax:len() > 0 then
				local values = {}
				values["cmd"] = cmd
				getClientParameterValues(syntax, strparams, values)
				if syntax:sub(1, 1) == '$' then
					-- magic
					local magicCmd, magicInfoSyntax = syntax:match("([^ ]+)[ ]?(.*)")
					local magicInfo = self:translate2(magicInfoSyntax, values)
					doMagic(self, magicCmd, magicInfo)
				else
					self:sendLine(self:translate2(syntax, values))
				end
			end
		else
			if strparams and strparams:len() > 0 then
				self:sendLine(cmd .. " " .. strparams)
			else
				self:sendLine(cmd)
			end
		end
	end
end

