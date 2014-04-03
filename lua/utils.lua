-- Copyright 2012-2014 Christopher E. Miller
-- License: GPLv3, see LICENSE file.


function class(base, init)
	local c = {}	 -- a new class instance
	if not init and type(base) == 'function' then
		init = base
		base = nil
	elseif type(base) == 'table' then
	 -- our new class is a shallow copy of the base class!
		for i,v in pairs(base) do
			c[i] = v
		end
		c._base = base
	end
	-- the class will be the metatable for all its objects,
	-- and they will look up their methods in it.
	c.__index = c
	-- expose a constructor which can be called by <classname>(<args>)
	local mt = {}
	mt.__call = function(class_tbl, ...)
		local obj = {}
		setmetatable(obj, c)
		if class_tbl.init then
			class_tbl.init(obj, ...)
		else
			-- if no init, call the base one.
			if base and base.init then
				base.init(obj, ...)
			end
		end
		return obj
	end
	if init then
		c.init = init
	end
	c.is_a = function(self, klass)
		local m = getmetatable(self)
		while m do
			if m == klass then return true end
			m = m._base
		end
		return false
	end
	setmetatable(c, mt)
	return c
end


-- Same as require but does not enforce it to exist.
-- Different from pcall(require, ...) because we still want errors for other reasons.
function include(mod)
  local searchers = package.loaders or package.searchers
  local i = #searchers
  searchers[i] = function(mod)
    return function()
      return { _includenotloaded = 42 }
    end
  end
  local ok, m, merr = pcall(require, mod)
  searchers[i] = nil
  -- assert(ok, m)
	if not ok then
		-- Not sure why this happens but it does.
		return false, m
	end
	assert(m, merr)
	if type(m) == "table" then
		if m._includenotloaded == 42 then
			return false, "include cannot find module " .. mod
		end
	end
  return m
end


event = class()

function event:init()
	local mt = getmetatable(self)
	mt.__call = function(e, ...)
		for i = 1, #e do
			local f = e[i]
			if type(f) == "string" then
				local s = f
				f = _G[s]
				if not f then
					-- error("Unable to call string as function, not an existing function: " .. s)
					error("Unable to call string as function, not a global function: " .. s)
				end
			end
			local x = f(...)
			if false == x then
				break
			end
			if "stop" == x then
				return "stop"
			end
		end
	end
end

-- Allows adding a function, or the name of a global function.
-- If the func already exists, it is not added again.
function event:add(func)
	assert(func)
	for i = 1, #self do
		if func == self[i] then
			return
		end
	end
	table.insert(self, func)
end

function event:remove(func)
	for i = 1, #self do
		if func == self[i] then
			table.remove(self, i)
			break;
		end
	end
end


-- Consideration for deprecation:
function iteratorstring(...)
	local s = ""
	for k, v in ... do
		if string.len(s) > 0 then
			s = s .. ", " .. k .. "="
		else
			s = k .. "="
		end
		if type(v) == "number" then
			s = s .. tostring(v)
		elseif type(v) == "string" then
			-- s = s .. "\"" .. replace(replace(v, "\\", "\\\\"), "\"", "\\\"") .. "\""
			s = s .. string.format("%q", v)
		else
			s = s .. tostring(v)
		end
	end
	return s
end

-- Consideration for deprecation:
function tablestring(t)
	-- assert(type(t) == "table")
	return iteratorstring(pairs(t))
end


-- Consideration for deprecation:
function itablestring(t)
	-- assert(type(t) == "table")
	return iteratorstring(ipairs(t))
end


-- Consideration for deprecation:
--[[
function try(tryFunc, catchFunc)
	local ok, err = pcall(tryFunc)
	if not ok then
		catchFunc(err)
	end
end
--]]
function try(tryFunc, catchFunc, finallyFunc)
	local ok, err = pcall(tryFunc)
	if not ok then
		if finallyFunc then
			try(function()
				catchFunc(err)
			end, function(e)
				finallyFunc()
				error(e)
			end)
		else
			catchFunc(err)
		end
	end
	if finallyFunc then
		finallyFunc()
	end
end


function isnan(x)
	x = tonumber(x)
	if x then
		return not (x >= 0 or x < 0)
	end
	return nil
end


--[[
function round(num, idp)
	return tonumber(string.format("%." .. (idp or 0) .. "f", num))
end
--]]
function round(num, idp)
	local shift = 10 ^ (idp or 0)
	return math.floor(num * shift + 0.5) / shift
end
assert(round(2.2, 2) == 2.2)
assert(round(2.2222, 2) == 2.22)
assert(round(2.266, 2) == 2.27)
assert(round(2.2, 3) == 2.2)
assert(round(2.222222, 3) == 2.222)
assert(round(2.555555, 3) == 2.556)
assert(round(-4.1999999999999, 2) == -4.20)
assert(round(-4.199999) == -4)
assert(round(-4.199999, 1) == -4.2)


function split(text, splitBy, plain, results)
	results = results or {}
	local pos = 1
	if "" == splitBy then -- this would result in endless loops
		return nil, "Empty splitBy"
	end
	while true do
		local first, last = string.find(text, splitBy, pos, plain)
		if first then
			table.insert(results, string.sub(text, pos, first - 1))
			pos = last + 1
		else
			table.insert(results, string.sub(text, pos))
			break
		end
	end
	return results
end
assert(#split("foo||bar", "||", true) == 2)
assert(split("foo||bar", "||", true)[1] == "foo")
assert(split("foo||bar", "||", true)[2] == "bar")




