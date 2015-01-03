-- Copyright 2012-2015 Christopher E. Miller, Anders Bergh
-- License: GPLv3, see LICENSE file.

-- Simply require this file and IrcClients get a nicklist.
-- Note: this must be done before any channels are joined.
-- Utility functions are provided, as well as:
-- client:nicklist(chan) - returns table: key=nick, value=table:
-- 	joined - optional, set to the time when they joined, or nil if they were here already.

local function case_insensitive_mt_for_client(client)
	return {
		__index = function(t, k)
			k = type(k) == "string" and client.tolower(k) or k
			return rawget(t, k)
		end,
		__newindex = function(t, k, v)
			k = type(k) == "string" and client.tolower(k) or k
			rawset(t, k, v)
		end
	}
end

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

-- preserve case of channel names
local original_channel_names = {}

function nl_make(client, channel)
	assert(not client._nicklists[channel])
	original_channel_names[client.tolower(channel)] = channel
	return setmetatable({}, case_insensitive_mt_for_client(client))
end

function nl_on_nick(client, prefix, cmd, params)
	local nick = nickFromSource(prefix)
	local newnick = params[1]
	for channel, nl in pairs(client._nicklists) do
		local value = nl[nick]
		value.nick = newnick
		nl[newnick], nl[nick] = nl[nick], nil
	end

	-- Fire artificial NICK_CHAN events per channel this guy is on,
	for channel, nl in pairs(client._nicklists) do
		if nl[newnick] then
			client.on["NICK_CHAN"](client, prefix, "NICK_CHAN", {channel, newnick})
		end
	end
end

function nl_on_part(client, prefix, cmd, params)
	local nick = nickFromSource(prefix)
	local channel = client:channelNameFromTarget(params[1])

	local nl = client._nicklists[channel]
	if nl then
		if nick == client:nick() then
			client._nicklists[channel] = nil
			original_channel_names[client.tolower(channel)] = nil
		else
			client._nicklists[channel][nick] = nil
		end
	end
end

function nl_on_join(client, prefix, cmd, params)
	local nick = nickFromSource(prefix)
	local channel = client:channelNameFromTarget(params[1])

	local nl = client._nicklists[channel]
	if not nl then
		client._nicklists[channel] = nl_make(client, channel)
	end
	client._nicklists[channel][nick] = { nick = nick, joined = os.time() }
end

function nl_on_quit(client, prefix, cmd, params)
	local nick = nickFromSource(prefix)
	if nick == client:nick() then
		-- It's me quitting, clear everything.
		client._nicklists = nil
	else
		local nick = nickFromSource(prefix)
		-- Fire artificial QUIT_CHAN events per channel this guy is on,
		for channel, nl in pairs(client._nicklists) do
			channel = original_channel_names[channel]
			if nl[nick] then
				client.on["QUIT_CHAN"](client, prefix, "QUIT_CHAN", {channel, params[1]})
				nl[nick] = nil
			end
		end
	end
end

function nl_on_kick(client, prefix, cmd, params)
	local channel = client:channelNameFromTarget(params[1])
	local nl = client._nicklists[channel]
	if nl then
		local kicked = params[2]
		nl[kicked] = nil
	end
end

function nl_on_353(client, prefix, cmd, params) -- NAMES info
	local channel = client:channelNameFromTarget(params[3])
	if not client._nicklists[channel] then
		-- print("Registering", chan);
		client._nicklists[chan] = nl_make(client, channel)
	end
	local nl = client._nicklists[channel]
	for xnick in params[4]:gmatch("[^ ]+") do
		local prefixes, nick = client:getNickInfo(xnick)
		if not nl[nick] then
			nl[nick] = { nick = nick }
		end
	end
end

local function nl_compatkeys(t)
	-- for backwards compatibility, preserve case of nick keys
	local tmp = {}

	for k, v in pairs(t) do
		tmp[v.nick] = v
	end

	return tmp
end

function regNickList(client)
	if client._nicklists then
		return -- Nicklists already setup for this client
		-- This can happen when reloading this file.
	end
	client._nicklists = setmetatable({}, case_insensitive_mt_for_client(client))
	client.nicklist = function(client, channel)
		local nl = client._nicklists[channel]
		if nl then
			return nl_compatkeys(nl)
		end
	end

	client.on["NICK"] = "nl_on_nick"
	client.on["PART"] = "nl_on_part"
	client.on["JOIN"] = "nl_on_join"
	client.on["QUIT"] = "nl_on_quit"
	client.on["KICK"] = "nl_on_kick"
	client.on["353"] = "nl_on_353"
end

-- Returns a table: {[channel] = {[nick] = {joined = ts}}}
function getNicklists(client)
	local tmp = {}
	for channel, nl in pairs(client._nicklists) do
		tmp[original_channel_names[channel]] = nl_compatkeys(nl)
	end
	return tmp
end

function getNickOnChannel(client, nick, channel)
	local nl = client:nicklist(channel)
	if nl and nl[nick] then
		return nl[nick], nl[nick].nick
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

