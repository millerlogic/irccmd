-- Copyright 2012-2014 Christopher E. Miller
-- License: GPLv2, see LICENSE file.

-- Base for timers.
-- Does not do anything unless timer_tick is called.
-- timer_loop can be used, which is a helper function for a sleep loop that calls timer_tick.
-- Timers should not catch-up, and should have resolution of 0.1 seconds or better.


require("utils")


Timer = class()
Timer_timers = Timer_timers or {}
Timer_timers_buf = Timer_timers_buf or {}

function Timer:init(timeout_seconds, timeout_function)
	self._timeout = tonumber(timeout_seconds, 10) or 0
	self._timeout_function = timeout_function
end

function Timer:start()
	if self._remain then return "Timer already started" end
	if self._timeout <= 0 then return "Invalid timer timeout value" end
	if type(self._timeout_function) ~= "function" then return "Invalid timer timeout callback function" end
	self._remain = self._timeout + 0.1 -- don't include the next partial tick.
	table.insert(Timer_timers, self)
	return "Started"
end

function Timer:stop()
	if not self._remain then return "Timer not started" end
	self._remain = nil
	for i = 1, #Timer_timers do
		if Timer_timers[i] == self then
			table.remove(Timer_timers, i)
			break
		end
	end
	return "Stopped"
end

function timer_tick(seconds)
	-- Clone the timers and use the clone to allow safe timer:stop() during a tick.
	local timers = Timer_timers_buf
	local tl = #Timer_timers_buf
	for i = 1, #Timer_timers do
		timers[i] = Timer_timers[i]
	end
	for i = #Timer_timers + 1, tl do
		timers[i] = nil
	end
	for i = 1, #timers do
		local t = timers[i]
		assert(t._remain)
		t._remain = t._remain - seconds
		if t._remain <= 0 then
			t._remain = t._timeout
			t:_timeout_function()
		end
	end
end


-- Helper function for a sleep loop processing timers.
-- sleepFunc is required, takes a number of seconds to sleep, which is sleepSeconds.
-- If sleepFunc returns "stop" the sleep loop ends.
-- sleepSeconds is the number of seconds to sleep, which should be 0.1 or better; defaults to 0.1
function timer_loop(sleepFunc, sleepSeconds)
	sleepSeconds = tonumber(sleepSeconds) or 0.1
	while true do
		if "stop" == sleepFunc(sleepSeconds) then break end
		timer_tick(sleepSeconds)
	end
	return "stop"
end

