#include "PathingLuaInternal.h"

#include "Globals.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <cstdint>
#include <cstring>

namespace PathingLuaDetail
{
	namespace
	{
		int I_Vector3(lua_State* L)
		{
			int arg = 1;
			if (lua_istable(L, 1) && lua_isnumber(L, 2))
				arg = 2;
			const float x = static_cast<float>(luaL_optnumber(L, arg, 0.0));
			const float y = static_cast<float>(luaL_optnumber(L, arg + 1, 0.0));
			const float z = static_cast<float>(luaL_optnumber(L, arg + 2, 0.0));
			PushVector3(L, x, y, z);
			return 1;
		}

		int I_Color(lua_State* L)
		{
			int arg = 1;
			if (lua_istable(L, 1) && lua_isnumber(L, 2))
				arg = 2;
			const int r = static_cast<int>(luaL_optinteger(L, arg, 255));
			const int g = static_cast<int>(luaL_optinteger(L, arg + 1, 255));
			const int b = static_cast<int>(luaL_optinteger(L, arg + 2, 255));
			const int a = static_cast<int>(luaL_optinteger(L, arg + 3, 255));
			const uint32_t argb =
				(static_cast<uint32_t>(a & 255) << 24) |
				(static_cast<uint32_t>(r & 255) << 16) |
				(static_cast<uint32_t>(g & 255) << 8) |
				static_cast<uint32_t>(b & 255);
			lua_pushinteger(L, static_cast<lua_Integer>(argb));
			return 1;
		}

		int I_Marker(lua_State* L)
		{
			/* I:Marker(attrs) → same as Pack:CreateMarker(attrs) */
			lua_getglobal(L, "Pack");
			if (!lua_istable(L, -1))
			{
				lua_pop(L, 1);
				lua_pushnil(L);
				return 1;
			}
			lua_getfield(L, -1, "CreateMarker");
			lua_remove(L, -2);
			if (!lua_isfunction(L, -1))
			{
				lua_pop(L, 1);
				lua_pushnil(L);
				return 1;
			}
			/* Pass attribute table if present */
			int attr = 0;
			if (lua_istable(L, 1) && lua_istable(L, 2))
				attr = 2;
			else if (lua_istable(L, 1) && !lua_isfunction(L, 1))
				attr = 1;
			if (attr)
				lua_pushvalue(L, attr);
			else
				lua_newtable(L);
			if (lua_pcall(L, 1, 1, 0) != LUA_OK)
			{
				LogLuaError(L, "I:Marker");
				lua_pop(L, 1);
				lua_pushnil(L);
			}
			return 1;
		}

		int I_Guid(lua_State* L)
		{
			int arg = lua_istable(L, 1) ? 2 : 1;
			const char* s = luaL_optstring(L, arg, "");
			lua_pushstring(L, s ? s : "");
			return 1;
		}

		int I_Texture(lua_State* L)
		{
			/* Stub — return path/id string for SetTexture consumers */
			int arg = lua_istable(L, 1) ? 2 : 1;
			if (lua_isnumber(L, arg))
			{
				lua_pushvalue(L, arg);
				return 1;
			}
			const char* path = luaL_optstring(L, arg, "");
			lua_pushstring(L, path ? path : "");
			return 1;
		}
	}

	void RegisterInstance(lua_State* L)
	{
		lua_newtable(L);
		lua_pushcfunction(L, I_Vector3);
		lua_setfield(L, -2, "Vector3");
		lua_pushcfunction(L, I_Color);
		lua_setfield(L, -2, "Color");
		lua_pushcfunction(L, I_Marker);
		lua_setfield(L, -2, "Marker");
		lua_pushcfunction(L, I_Guid);
		lua_setfield(L, -2, "Guid");
		lua_pushcfunction(L, I_Texture);
		lua_setfield(L, -2, "Texture");
		lua_setglobal(L, "I");
	}
}
