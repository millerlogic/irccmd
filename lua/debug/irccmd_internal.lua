-- Copyright 2012-2014 Christopher E. Miller
-- License: GPLv2, see LICENSE file.

-- Below are stubs for all functions provided.
-- They can be used for testing as well as documentation.


if true then
	internal = {} -- Deprecated global
	internal._icDebug = true

	--[[
	assert = function(expr, msg)
		if not expr then
			if msg then
				print("An assertion failed: " .. msg)
			else
				print("An assertion failed")
			end
		end
		return expr
	end
	--]]

	-- limit is optional, exclusive upper bound
	internal.random = function(limit)
		assert(limit == nil or limit > 0, "invalid random limit " .. limit);
		if limit then
			return limit - 1
		end
		return 4200000
	end

	-- lower is inclusive, upper is exclusive.
	-- x = frandom()
	-- x = frandom(upper)
	-- x = frandom(lower, upper)
	internal.frandom = function(a, b)
		if b then
			assert(b > a, "invalid frandom bounds " .. a .. ".." .. b);
			return b - 1
		elseif a then
			assert(a > 0, "invalid frandom upper bound " .. a);
			return a - 1
		end
		return 4200000
	end

	-- milliseconds() is not very accurate and is subject to overflow!
	internal.milliseconds = function()
		return 55555
	end

	-- milliseconds_diff() handles overflow as long as the times don't span longer than 49 days.
	internal.milliseconds_diff = function(old, new)
		return 1000
	end

	local function print_args(f, ...)
		local n = select('#', ...)
		for i = 1, n do
			local v = select(i, ...)
			if v ~= nil then
				f:write(tostring(v))
			end
		end
	end

	internal.console_print = function(...)
		print_args(io.stdout, ...)
	end

	internal.console_print_err = function(...)
		print_args(io.stderr, ...)
	end

	-- line = irc_input(line_data)
	internal.irc_input = function(s)
		return s
	end

	debugPPRIVMSG = debugPPRIVMSG or "Hey (test data)"

	-- (prefix, cmd, params) = irc_input(line)
	-- params is a table; prefix may be nil if no prefix.
	internal.irc_parse = function(s)
		if s:find("^:server%.name 001 ") then
			return "server.name", "001", { "SelfNick", "Welcome (test data)" }
		end
		return "OtherUser!hello@hi.com", "PRIVMSG", { "#foo", debugPPRIVMSG }
		-- return "server.name", "318", { "SomeUser1", "OtherUser", "End of /WHOIS list." }
	end

	internal.compare_ascii = function(s1, s2)
		if s1 == s2 then return 0 end
		return s
	end

	internal.compare_rfc1459 = function(s1, s2)
		if s1 == s2 then return 0 end
		return 1
	end

	internal.compare_strict_rfc1459 = function(s1, s2)
		if s1 == s2 then return 0 end
		return 1
	end

	internal.tolower_ascii = string.lower
	internal.tolower_rfc1459 = string.lower
	internal.tolower_strict_rfc1459 = string.lower

	internal.memory_limit = function()
		return 0, 0
	end

	internal.socket_connect = function(addr, port, stype, family, socketCreatedFunc)
		if type(stype) == "function" then
			socketCreatedFunc = stype
			stype = nil
			family = nil
		elseif type(family) == "function" then
			socketCreatedFunc = family
			family = nil
		end
		if socketCreatedFunc then
			assert(type(socketCreatedFunc) == "function")
			socketCreatedFunc(6)
			socketCreatedFunc(71)
		end
		return 71
	end

	internal.socket_bind = function(socket, address, port , stype, family)
		assert(sock == 71)
		assert(address or port)
		return true
	end

	--[[
	internal.socket_listen2 = function(socket, backlog)
		assert(sock == 71)
		assert(not backlog or tonumber(backlog))
		return true
	end
	--]]

	-- socket = socket_listen([address,] port, [backlog, [, type, family]])
	internal.socket_listen = function(...)
		return 71
	end

	local acceptcount = 0

	internal.socket_accept = function(socket)
		acceptcount = acceptcount + 1
		if acceptcount < 2 then
			assert(sock == 71)
			return 71, "127.0.0.1", 39393
		end
		return nil, "Test error message from internal.socket_accept()", nil
	end

	internal.socket_close = function(sock)
		assert(sock == 71)
	end

	internal.socket_blocking = function(sock, byes)
		assert(sock == 71, "sock should be 71 not " .. tostring(sock))
		assert(type(byes) == "boolean")
	end

	internal.socket_linger = function(sock, seconds)
		assert(sock == 71)
		assert(seconds == false or (type(seconds) == "number" and seconds >= 0))
	end

	internal.socket_shutdown = function(sock, how)
		assert(sock == 71)
		assert(how:upper() == "RECEIVE" or how:upper() == "SEND" or how:upper() == "BOTH")
	end

	internal.socket_send = function(sock, data, flags)
		assert(sock == 71)
		assert(type(data) == "string")
		assert(not flags or type(flags) == "string")
		return string.len(data)
	end

	internal.socket_receive = function(sock, flags, maxBytes)
		assert(sock == 71)
		assert(not flags or type(flags) == "string")
		assert(not maxBytes or type(maxBytes) == "number")
		return ":server.name 001 SelfNick :Welcome (test data)\r\n"
			.. ":OtherUser!hello@hi.com PRIVMSG #foo :" .. debugPPRIVMSG .. "\r\n"
	end

	local selectcount = 0

	internal.socket_select = function(sockets, microseconds)
		assert(type(sockets) == "table")
		assert(not microseconds or type(microseconds) == "number")
		selectcount = selectcount + 1
		if selectcount <= 2 then
			if sockets[71] then
				if sockets["stdin"] and selectcount == 2 then
					-- return { ["stdin"] = "/echo Test echo from console!\r\n" }
					return { ["stdin"] = "#foo Test message from console!\r\n" }
				end
				return { [71] = sockets[71] }
			end
		elseif selectcount == 3 and microseconds and microseconds >= 0 then
			return "timeout"
		end
		return nil, "Test error message from internal.socket_select()", 55555
	end

end

return internal
