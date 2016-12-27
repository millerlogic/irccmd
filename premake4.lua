solution "irccmd"
	configurations { "Debug", "Release" }

	project "irccmd"
		kind "SharedLib"
		language "C"
		files { "**.h", "**.c" }

		targetname "irccmd_internal"
		targetprefix ""

		-- Find the proper Lua pkg-config in this order.
		local lua_names = { "lua5.1", "luajit", "lua" }

		local lua_name
		for _, name in ipairs(lua_names) do
			if os.execute("pkg-config "..name) == 0 then
				lua_name = name
				break
			end
		end

		if lua_name then
			buildoptions { "`pkg-config "..lua_name.." --cflags`" }
			linkoptions { "`pkg-config "..lua_name.." --libs`" }
		else
			-- no pkg-config info installed, just try Lua
			links { "lua" }
		end

		configuration "Debug"
			defines { "_DEBUG" }
			flags { "Symbols" }

		configuration "Release"
			flags { "Optimize" }

		configuration "macosx"
			targetextension ".so"
