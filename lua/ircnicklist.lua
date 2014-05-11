-- Copyright 2012-2014 Christopher E. Miller
-- License: GPLv3, see LICENSE file.

-- Simply require this file and IrcClients get a nicklist.
-- Note: this must be done before any channels are joined.
-- Utility functions are provided, as well as:
-- client:nicklist(chan) - returns table: key=nick, value=table:
-- 	joined - optional, set to the time when they joined, or nil if they were here already.


-- if asString is true, a string is returned, otherwise a table.
-- stringDelimiter defaults to space.
-- Always returns a new table/string.
function getSortedNickList(client, channel, asString, stringDelimiter)
	local nl = client:nicklist(channel)
	if nl then
		local result = {}
		for k, v in pairs(nl) do
			table.insert(result, k)
		end
		table.sort(result, function(a, b)
			return client.strcmp(a, b) < 0
		end)
		if asString then
			stringDelimiter = stringDelimiter or ' '
			local s = ''
			for i = 1, #result do
				if i > 1 then
					s = s .. stringDelimiter
				end
				s = s .. result[i]
			end
			return s
		end
		return result
	end
end


function isOnChannel(client, nick, channel)
	local nl = client:nicklist(channel)
	if nl then
		for k, v in pairs(nl) do
			if 0 == client.strcmp(nick, k) then
				return true
			end
		end
	end
	return false
end


local function nl_getkey(client, channel)
	if client._nicklists[channel] then
		return channel
	end
	for k, v in pairs(client._nicklists) do
		if 0 == client.strcmp(channel, k) then
			return k
		end
	end
end

function nl_make(client, channel)
	assert(not client._nicklists[channel])
	local nl = {}
	local nlmt = {}
	nlmt.__index = function(t, key)
		local x = rawget(t, key)
		if x then return x end
		if type(key) == "string" then
			for k, v in pairs(t) do
				if 0 == client.strcmp(k, key) then
					return v
				end
			end
		end
	end
	setmetatable(nl, nlmt)
	return nl
end

function nl_on_nick(client, prefix, cmd, params)
	local nick = nickFromSource(prefix)
	local newnick = params[1]
	for k, v in pairs(client._nicklists) do
		local key = k
		if key then
			local value = client._nicklists[key][nick]
			if value then
				client._nicklists[key][nick] = nil
				client._nicklists[key][newnick] = value
			end
		end
	end
	-- Fire artificial NICK_CHAN events per channel this guy is on,
	for k, v in pairs(client._nicklists) do
		if k and v then
			if v[newnick] then
				local x_chan_params = {}
				x_chan_params[1] = k
				x_chan_params[2] = newnick
				client.on["NICK_CHAN"](client, prefix, "NICK_CHAN", x_chan_params)
			end
		end
	end
end

function nl_on_part(client, prefix, cmd, params)
	local nick = nickFromSource(prefix)
	local key = nl_getkey(client, client:channelNameFromTarget(params[1]))
	if key then
		if nick == client:nick() then
			client._nicklists[key] = nil
		else
			client._nicklists[key][nick] = nil
		end
	end
end

function nl_on_join(client, prefix, cmd, params)
	local nick = nickFromSource(prefix)
	local chan = client:channelNameFromTarget(params[1])
	local key = nl_getkey(client, chan)
	if not key then
		client._nicklists[chan] = nl_make(client, chan)
		key = chan
	end
	if key then
		client._nicklists[key][nick] = { joined = os.time() }
	end
end

function nl_on_quit(client, prefix, cmd, params)
	local nick = nickFromSource(prefix)
	if nick == client:nick() then
		-- It's me quitting, clear everything.
		client._nicklists = {}
	else
		local nick = nickFromSource(prefix)
		-- Fire artificial QUIT_CHAN events per channel this guy is on,
		for k, v in pairs(client._nicklists) do
			if k and v then
				if v[nick] then
					local x_chan_params = {}
					x_chan_params[1] = k
					x_chan_params[2] = params[1]
					client.on["QUIT_CHAN"](client, prefix, "QUIT_CHAN", x_chan_params)
				end
			end
		end
		for k, v in pairs(client._nicklists) do
			local key = k
			if key then
				client._nicklists[key][nick] = nil
			end
		end
	end
end

function nl_on_kick(client, prefix, cmd, params)
	local key = nl_getkey(client, client:channelNameFromTarget(params[1]))
	if key then
		local kicked = params[2]
		client._nicklists[key][kicked] = nil
	end
end

function nl_on_353(client, prefix, cmd, params) -- NAMES info
	local chan = client:channelNameFromTarget(params[3])
	local key = nl_getkey(client, chan)
	if not key then
		-- print("Registering", chan);
		client._nicklists[chan] = nl_make(client, chan)
		key = chan
	end
	if key then
		for xnick in params[4]:gmatch("[^ ]+") do
			local prefixes, nick = client:getNickInfo(xnick)
			if not client._nicklists[key][nick] then
				client._nicklists[key][nick] = { }
			end
		end
	end
end

function regNickList(client)
	if client._nicklists then
		return -- Nicklists already setup for this client
		-- This can happen when reloading this file.
	end
	client._nicklists = {}
	local _nlsmt = {}
	_nlsmt.__index = function(t, key)
		local x = rawget(t, key)
		if x then return x end
		if type(key) == "string" then
			for k, v in pairs(t) do
				if 0 == client.strcmp(k, key) then
					return v
				end
			end
		end
	end
	setmetatable(client._nicklists, _nlsmt)

	client.nicklist = function(client, channel)
		local key = nl_getkey(client, channel)
		-- print("channel", channel, " = key", key)
		if key then
			return client._nicklists[key]
		end
		return nil
	end

	client.on["NICK"] = "nl_on_nick"
	client.on["PART"] = "nl_on_part"
	client.on["JOIN"] = "nl_on_join"
	client.on["QUIT"] = "nl_on_quit"
	client.on["KICK"] = "nl_on_kick"
	client.on["353"] = "nl_on_353"

end

-- Returns a table: key is channel name, value is (table: key=nickname, value=table).
function getNicklists(client)
	return client._nicklists
end

function getNickOnChannel(client, nick, channel)
	local nl = client:nicklist(channel)
	if nl then
		for n, ninfo in pairs(nl) do
			if 0 == client.strcmp(nick, n) then
				return ninfo, n
			end
		end
	end
end

if irccmd then
	clientAdded:add("regNickList")

	for i, client in ipairs(ircclients) do
		regNickList(client)
	end

	--[[
	function unregNickList(client)

	end

	if clientRemoved then
		clientRemoved:add("unregNickList")
	end
	--]]
end

