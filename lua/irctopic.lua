local function get_topic_table(client, channel)
	local t = client._topics[channel] or {}
	client._topics[channel] = t
	return t
end

-- :foo!bar@baz TOPIC #channel :Hello dudes
function ts_on_topic(client, prefix, cmd, params)
	local nick = nickFromSource(prefix)
	local t = get_topic_table(client, client:channelNameFromTarget(params[1]))
	t.topic = params[#params]
	t.set_by = nickFromSource(prefix)
	t.ts = os.time()
end

-- :server.foo 332 ludebot #channel :This is the topic
function ts_on_332(client, prefix, cmd, params)
	local t = get_topic_table(client, client:channelNameFromTarget(params[2]))
	t.topic = params[#params]
end

-- :server.foo 333 ludebot #channel byte[] 1234567890
function ts_on_333(client, prefix, cmd, params)
	local t = get_topic_table(client, client:channelNameFromTarget(params[2]))
	t.set_by = params[3]
	t.ts = tonumber(params[4])
end

function regTopic(client)
	if client._topics then
		return
	end
	client._topics = {}

	local mt = {}
	mt.__index = function(t, k)
		k = type(k) == "string" and client.tolower(k) or k
		return rawget(t, k)
	end
	mt.__newindex = function(t, k, v)
		k = type(k) == "string" and client.tolower(k) or k
		rawset(t, k, v)
	end

	setmetatable(client._topics, mt)

	client.topic = function(client, channel)
		local t = client._topics[channel]
		if t then
			return t.topic, t.set_by, t.ts
		end
	end

	client.on["TOPIC"] = "ts_on_topic"
	client.on["332"] = "ts_on_332"
	client.on["333"] = "ts_on_333"
end

if irccmd then
	clientAdded:add("regTopic")

	for i, client in ipairs(ircclients) do
		regTopic(client)
	end
end

