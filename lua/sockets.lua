-- Copyright 2012-2014 Christopher E. Miller
-- License: GPLv2, see LICENSE file.

require("internal")
require("utils")


SocketBase = class()

function SocketBase:init(socket)
	-- print("Initializing SocketBase")
	self._sock = socket or "N/A"
end

function SocketBase:blocking(byes)
	return internal.socket_blocking(self._sock, byes)
end

function SocketBase:destroy()
	internal.socket_close(self._sock)
	self._sock = "N/A"
end

-- Lower level. Called when the socket can read.
function SocketBase:onCanRead()
end

-- Lower level. Returns true when wanting to read. Assumed always true.
function SocketBase:needRead()
	return true
end

-- Lower level. Called when the socket can write.
function SocketBase:onCanWrite()
	return false
end

-- Lower level. Returns true when wanting to write.
function SocketBase:needWrite()
	return false
end

function SocketBase:valid()
	return nil
end

--[[
function SocketBase:reuseaddr(byes)
	return internal.socket_reuseaddr(self._sock, byes)
end
--]]


SocketClient = class(SocketBase)

function SocketClient:init(socket)
	SocketBase.init(self, socket)
	-- print("Initializing SocketClient")
	self._sendbuf = ""
	self._dis = type(socket) ~= "number"
end

function SocketClient:valid()
	return not self._dis
end

function SocketClient:blocking(byes)
	assert(type(byes) == "boolean")
	self._blocking = byes
	if self:valid() then
		return SocketBase.blocking(self, byes)
	end
	return true
end

-- connect(address, port [, type, family])
-- function SocketClient:connect(...)
function SocketClient:connect(address, port, stype, sfamily)
	assert(not tonumber(self._sock))
	assert(type(stype) ~= "function")
	assert(type(sfamily) ~= "function")
	-- local sock, errmsg, errcode = internal.socket_connect(...)
	local createfunc
	if self._blocking == false then
		createfunc = function(sock)
			internal.socket_blocking(sock, false)
		end
	end
	local sock, errmsg, errcode = internal.socket_connect(address, port, stype, sfamily, createfunc)
	if not sock then
		return nil, errmsg, errcode
	end
	self._dis = nil
	self._sock = sock
	self:setConnected()
	return true
end

function SocketClient:send(data)
	-- assert(type(data) == "string")
	if string.len(self._sendbuf) > 0 then
		self._sendbuf = self._sendbuf .. data
		--[[
		if string.len(self._sendbuf) > 8192 then
			io.stderr:write("SocketClient send buffer is full\n")
			self._sendbuf = self._sendbuf:sub(1, 8192)
		end
		--]]
	else
		local sent = internal.socket_send(self._sock, data)
		if not sent or sent < 1 then
			self._sendbuf = self._sendbuf .. data
		else
			if string.len(data) ~= sent then
				self._sendbuf = self._sendbuf .. data:sub(sent + 1)
			end
		end
	end
end

function SocketClient:linger(seconds)
	return internal.socket_linger(self._sock, seconds)
end

-- Lower level. Called when the socket can send again.
-- Returns true if more data needs to be sent.
function SocketClient:onCanWrite()
	if string.len(self._sendbuf) > 0 then
		local nbytes, xmsg, xx = internal.socket_send(self._sock, self._sendbuf)
		if not nbytes then
			self._sendbuf = "";
			self:setDisconnected(xmsg, xx)
			return "-"
		else
			if string.len(self._sendbuf) == nbytes then
				self._sendbuf = ""
			else
				self._sendbuf = self._sendbuf:sub(nbytes + 1)
				return true
			end
		end
	end
	return false
end

-- Lower level. Returns true when more data to send.
function SocketClient:needWrite()
	-- return string.len(self._sendbuf) > 0
	return self._sendbuf ~= ""
end

-- Lower level. Called when the socket can read.
function SocketClient:onCanRead()
	local data, xmsg, xx = internal.socket_receive(self._sock)
	if not data then
		if data == false then
			-- Connection closed.
			self:setDisconnected()
		else
			-- Error.
			self:setDisconnected(xmsg, xx)
		end
		return "-"
	end
	-- internal.console_print("INPUT: ", data, "\n");
	self:onReceive(data)
end

-- Disconnect sends/receives on the socket.
-- destroy() must be called to release resources.
-- errmsg should not be set unless an error should be reported.
function SocketClient:disconnect(errmsg)
	-- internal.socket_shutdown(self._sock, "BOTH")
	internal.socket_shutdown(self._sock, "RECEIVE")
	self:setDisconnected(errmsg)
end

function SocketClient:onReceive(data)
end

function SocketClient:onConnected()
end

-- Lower level.
function SocketClient:setConnected()
	self:onConnected()
end

-- msg is nil for normal connection closed, otherwise error information.
-- Default is to write to stderr.
function SocketClient:onDisconnected(msg, code)
	if msg then
		-- io.stderr:write("Socket " .. (self._sock or "") .. " error: ", msg, " ", (code or ""), "\n")
		io.stderr:write("Socket error: ", msg, " ", (code or ""), "\n")
	else
		-- io.stderr:write("Socket " .. (self._sock or "") .. " disconnected\n")
		-- io.stderr:write("Socket disconnected\n")
	end
end

-- Lower level.
function SocketClient:setDisconnected(msg, code)
	if self._dis then return false end
	self._dis = true
	self:onDisconnected(msg, code)
	return true
end


SocketServer = class(SocketBase)

function SocketServer:init(socket)
	SocketBase.init(self, socket)
	-- print("Initializing SocketServer")
end

function SocketServer:valid()
	return type(self._sock) == "number"
end

-- listen([address,] port, [backlog, [, type, family]])
function SocketServer:listen(...)
	local sock, errmsg, errcode = internal.socket_listen(...)
	if not sock then
		return nil, errmsg, errcode
	end
	self._sock = sock
	return true
end

-- Construct a new socket client object.
function SocketServer:accepting(new_sock)
	return SocketClient(new_sock)
end

function SocketServer:accept()
	local new_sock, address, port = internal.socket_accept(self._sock)
	if not new_sock then
		return new_sock, address, port -- (nil, errmsg, errcode)
	end
	local newSocketObj = self:accepting(new_sock)
	assert(new_sock == newSocketObj._sock)
	self:onAccept(newSocketObj, address, port)
	return newSocketObj, address, port
end

-- A client was accepted.
function SocketServer:onAccept(newSocketObj, address, port)
end

-- Lower level. Called when the socket can accept a client.
function SocketServer:onCanRead()
	if not self:accept() then -- Calls onAccept.
		return "-"
	end
end


SocketClientLines = class(SocketClient)

function SocketClientLines:init(socket)
	SocketClient.init(self, socket)
	-- print("Initializing SocketClientLines")
	self._linebuf = ""
end

-- The line variable does not contain newline characters.
function SocketClientLines:onReceiveLine(line)
end

local function _checksocklinebuf(self)
	while true do
		local one, two, line = self._linebuf:find("([^\r\n]*)\r?\n")
		if line then
			self._linebuf = self._linebuf:sub(two + 1)
			self:onReceiveLine(line)
		else
			break
		end
	end
end

function SocketClientLines:onReceive(data)
	-- internal.console_print("INPUT: ", data, "\n");
	self._linebuf = self._linebuf .. data
	_checksocklinebuf(self)
end

function SocketClientLines:setDisconnected(msg, code)
	-- If any data in the _linebuf not line terminated, count it as a line:
	if string.len(self._linebuf) > 0 then
		_checksocklinebuf(self) -- check for full lines in case disconnected while receiving.
		-- now try a trailing line without line terminator:
		local line = self._linebuf:gsub("\r$", "") -- remove a trailing \r.
		self._linebuf = ""
		self:onReceiveLine(line)
	end
	SocketClient.setDisconnected(self, msg, code)
end

-- "\n" newline character will be appended.
function SocketClientLines:sendLine(line)
	return self:send(line .. "\n")
end


SelectManagerBase = class()

function SelectManagerBase:init()
	self._events = {}
end

-- If this function is overridden, standard input is automatically read.
function SelectManagerBase:onStandardInput(input)
end

function SelectManagerBase:onRead(sock)
end

function SelectManagerBase:onWrite(sock)
end

local function addsseventletter(events, sock, letter)
	if not events[sock] then
		events[sock] = letter
	else
		events[sock] = events[sock]:gsub(letter, "") .. letter
	end
end

local function removesseventletter(events, sock, letter)
	if events[sock] then
		local v = events[sock]:gsub(letter, "")
		if v == "" then
			v = nil
		end
		events[sock] = v
	end
end

-- Register the socket for receive/accept events.
function SelectManagerBase:addRead(sock)
	assert(type(sock) == "number")
	addsseventletter(self._events, sock, 'r')
end

function SelectManagerBase:removeRead(sock)
	assert(type(sock) == "number")
	removesseventletter(self._events, sock, 'r')
end

-- Register the socket for send events.
function SelectManagerBase:addWrite(sock)
	assert(type(sock) == "number")
	addsseventletter(self._events, sock, 'w')
end

function SelectManagerBase:removeWrite(sock)
	assert(type(sock) == "number")
	removesseventletter(self._events, sock, 'w')
end

-- stop() breaks out of the current loop()
-- stop("all") breaks out of all loop() calls on this SelectManagerBase.
function SelectManagerBase:stop(all)
	self._stop = true
	self._stopAll = (all == "all") or self._stopAll
end

function SelectManagerBase:onBeforeSelect()
end

-- If timers were included, they are automatically handled while in the select loop.
-- Any errors raised from events (socket, timer, stdin) break out of the loop function.
-- Returns true if the loop can be re-entered.
function SelectManagerBase:loop()
	-- self._stop = nil
	self._stop = self._stopAll

	if self.onStandardInput == SelectManagerBase.onStandardInput then
		self._events["stdin"] = nil
	else
		self._events["stdin"] = "r"
	end

	local lasttime = nil -- for timer_tick
	local tickresolution = 100

	while not self._stop do
		local microwait = -1
		if timer_tick then
			local now = internal.milliseconds()
			if not lasttime then
				lasttime = now
				microwait = tickresolution * 1000
			else
				local tdiff = internal.milliseconds_diff(lasttime, now)
				if tdiff >= tickresolution then
					lasttime = now
					microwait = tickresolution * 1000
					timer_tick(tickresolution / 1000)
				else
					microwait = (tickresolution - tdiff) * 1000
					assert(microwait > 0)
				end
			end
		else
			lasttime = nil
		end
		--[[ if microwait ~= -1 then
			io.stderr:write(" t=" .. microwait .. " ")
		end --]]
		self:onBeforeSelect()
		--[[
		local nevents = 0
		for k, v in pairs(self._events) do
			nevents = nevents + 1
		end
		print("", "select with " .. nevents .. " events, timeout = " .. microwait)
		--]]
		local selresult, xmsg, xerrcode = internal.socket_select(self._events, microwait);
		if not selresult then
			if xerrcode then
				error(xmsg .. " [" .. xerrcode .. "]")
			end
			error(xmsg)
		end
		if selresult ~= "timeout" then
			for k, v in pairs(selresult) do
				if k == "stdin" then
					self:onStandardInput(v)
				else
					for i = 1, string.len(v) do
						local ch = v:sub(i, i)
						if ch == 'r' then
							self:onRead(k)
						elseif ch == 'w' then
							self:onWrite(k)
						end
					end
				end
			end
		end
	end

	return not self._stopAll
end


SelectManager = class(SelectManagerBase)

function SelectManager:init()
	SelectManagerBase.init(self)
	self._sockets = {}
	self._maxSockets = 64
	self._numSockets = 0
end

-- When a socket receives onDisconnected, it should be removed from the SelectManager.
function SelectManager:add(socketObj)
	assert(tonumber(socketObj._sock), "Socket must be ready to receive events")
	assert(socketObj:valid(), "Socket does not return as valid")
	if not self._sockets[socketObj._sock] then
		if self._numSockets + 1 > self._maxSockets then
			error("Max sockets reached for SelectManager")
		end
		self._numSockets = self._numSockets + 1
	end
	self._sockets[socketObj._sock] = socketObj
end

function SelectManager:remove(socketObj)
	local sock
	if tonumber(socketObj._sock) then
		sock = socketObj._sock
	else
		-- Need to find it.
		for k, v in pairs(self._sockets) do
			if v == socketObj then
				sock = k
				break
			end
		end
	end
	if sock then
		if self._sockets[sock] then
			self._numSockets = self._numSockets - 1
		end
		self._sockets[sock] = nil
		self._events[sock] = nil
	end
end

function SelectManager:onBeforeSelect()
	-- local ccc = 0
	for sock, socketObj in pairs(self._sockets) do
		-- ccc = ccc + 1
		-- Note: always writing before reading.
		if socketObj:valid() == false then
			self:remove(socketObj)
			error("Invalid socket found in SelectManager")
		end
		if socketObj:needWrite() then
			-- print("", "w", sock)
			self._events[sock] = "w"
		elseif socketObj:needRead() then
			-- print("", "r", sock)
			self._events[sock] = "r"
		else
			-- print("", "no-event", sock)
			self._events[sock] = nil
		end
	end
	-- print("#", ccc, "sockets waiting for events")
end

-- Lower level.
function SelectManager:onRead(sock)
	local socketObj = self._sockets[sock]
	if socketObj then
		if "-" == socketObj:onCanRead() then
			self:remove(socketObj)
		end
	end
end

-- Lower level.
function SelectManager:onWrite(sock)
	local socketObj = self._sockets[sock]
	if socketObj then
		if "-" == socketObj:onCanWrite() then
			self:remove(socketObj)
		end
	end
end

