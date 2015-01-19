-- Copyright 2012-2014 Christopher E. Miller
-- License: GPLv2, see LICENSE file.

-- Timer line queue, for queueing lines to be sent via a timer.


require("utils")
require("timers")


function _tsl_qetype(v)
	if type(v) == "table" then
		return v.p
	else
		return nil
	end
end

function _tsl_qcount(queue, p)
	local n = 0
	if type(p) == "string" then
		for i, v in ipairs(queue) do
			if type(v) == "table" then
				if v.p == p then
					n = n + 1
				end
			end
		end
	else
		for i, v in ipairs(queue) do
			if type(v) == "string" then
				n = n + 1
			end
		end
	end
	return n
end

-- Inserts after its own type or after type with less or same occurences in the queue.
function _tsl_insert(queue, insert)
	local instype = _tsl_qetype(insert)
	local inscount = _tsl_qcount(queue, instype)
	local insindex = 1
	local i = #queue
	while i >= 1 do
		local v = queue[i]
		local vtype = _tsl_qetype(v)
		-- print("instype=",instype, "vtype=",vtype, "_tsl_qcount(queue, vtype)=",_tsl_qcount(queue, vtype), "inscount=",inscount)
		if instype == vtype or _tsl_qcount(queue, vtype) <= inscount then
			insindex = i + 1
			break
		end
		i = i - 1
	end
	table.insert(queue, insindex, insert)
	print("  inserted " .. (instype or "(string)") .. " at index " .. insindex .. " (this type occurs " .. (inscount + 1) .. " times)")
	return insindex
end

function _timersendline(client, line, priority)
	--[[
	if priority == true then
		-- No max for priority (for now)
		table.insert(client._queuelines1st, line)
		return
	end
	--]]
	if priority == true then
		-- Instead of actually giving it high priority, let's just call it "true".
		-- This will give other messages a fighting chance if too many priority=true.
		priority = "true"
	end
	if #client._queuelines >= client._queuelinesMax then
		if _tsl_qcount(client._queuelines, priority) >= client._queuelinesMax then
			io.stderr:write("Too many lines in queue, unable to add: ", line, "\n")
			return
		end
	end
	if type(priority) == "string" then
		local v = {}
		v.line = line
		v.p = priority
		-- table.insert(client._queuelines, v)
		_tsl_insert(client._queuelines, v)
	else
		-- table.insert(client._queuelines, line)
		_tsl_insert(client._queuelines, line)
	end
end

-- client is SocketClientLines or derived, or anything with sendLine(self, line)
-- maxQueue defaults to 128
-- burst is optional; number of lines that can burst at half the timeout_seconds.
function enableSendLineTimer(client, timeout_seconds, maxQueue, burst)
	assert(client)
	if type(client.sendLine) ~= "function" then return "This client does not have a sendLine function" end
	timeout_seconds = tonumber(timeout_seconds, 10) or 0
	if timeout_seconds <= 0 then return "Invalid timer timeout value" end
	if client._queuelines then return "sendLine timer already enabled for this client" end
	burst = tonumber(burst, 10) or 0
	if burst % 2 == 1 then
		-- Odd would let an extra burst, so make it one less for strictness.
		burst = burst - 1
	end
	maxQueue = maxQueue or 128
	client._queuelines = {} -- normal queue
	client._queuelines1st = {} -- priority queue - no max (for now)
	client._queuelinesMax = maxQueue
	client.sendLineNow = client.sendLine
	client.sendLine = _timersendline
	client._slqBurst = burst
	client._slqBurstTime = 0
	client._slqTime = 0
	client._slqTimer = Timer(timeout_seconds / 2, function(timer)
			if not client._queuelines then
				timer:stop()
				return
			end
			client._slqTime = client._slqTime + 1
			if #client._queuelines1st == 0 and #client._queuelines == 0 then
				return
			end
			local can
			if client._slqTime % 2 == 0 then
				can = true
			else
				if client._slqBurstTime > client._slqTime then
					if client._slqBurstTime < client._slqTime + (client._slqBurst * client._slqBurst) then
						can = true
						print("BURSTING: 2")
					end
				else
					client._slqBurstTime = client._slqTime
					can = true
					print("BURSTING: 1")
				end
			end
			if can then
				client._slqBurstTime = client._slqBurstTime + client._slqBurst * 2
				if client._slqBurstTime > client._slqTime + (client._slqBurst * client._slqBurst * 10) then
					client._slqBurstTime = client._slqTime + (client._slqBurst * client._slqBurst * 10)
				end
				local ln
				if #client._queuelines1st > 0 then
					ln = client._queuelines1st[1]
					table.remove(client._queuelines1st, 1)
				elseif #client._queuelines > 0 then
					ln = client._queuelines[1]
					table.remove(client._queuelines, 1)
				end
				if ln then
					if type(ln) == "table" then
						ln = ln.line
					end
					client:sendLineNow(ln)
				end
			end
		end)
	return client._slqTimer:start()
end

function disableSendLineTimer(client)
	if client._queuelines then
		client.sendLine = client.sendLineNow
		-- client.sendLineNow = nil
		client._queuelines = nil
		client._queuelines1st = nil
		client._queuelinesMax = nil
		client._slqTimer:stop()
		client._slqTimer = nil
		client._slqBurst = nil
		client._slqBurstTime = nil
		client._slqTime = nil
	end
end
