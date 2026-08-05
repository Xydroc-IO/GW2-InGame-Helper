#include "PathingLuaInternal.h"

#include "Globals.h"
#include "PathingTrails.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace PathingLuaDetail
{
	namespace
	{
		int World_Print(lua_State* L)
		{
			/* World.Print or World:Print */
			const int arg = lua_istable(L, 1) ? 2 : 1;
			const char* s = luaL_optstring(L, arg, "");
			if (G::API && G::API->Log)
				G::API->Log(LOGL_INFO, ADDON_NAME, s ? s : "");
			return 0;
		}

		int World_MarkerByGuid(lua_State* L)
		{
			const int arg = lua_istable(L, 1) ? 2 : 1;
			const char* guid = luaL_checkstring(L, arg);
			PathingTrails::Marker* m = FindMarkerByGuid(guid);
			if (!m)
			{
				lua_pushnil(L);
				return 1;
			}
			PushMarker(L, m);
			return 1;
		}

		int World_PathableByGuid(lua_State* L)
		{
			return World_MarkerByGuid(L);
		}

		int World_CategoryByType(lua_State* L)
		{
			const int arg = lua_istable(L, 1) ? 2 : 1;
			const char* type = luaL_checkstring(L, arg);
			lua_newtable(L);
			lua_pushstring(L, type);
			lua_setfield(L, -2, "Namespace");
			lua_pushstring(L, type);
			lua_setfield(L, -2, "Name");
			return 1;
		}

		int World_GetClosestMarkers(lua_State* L)
		{
			int arg = 1;
			if (lua_istable(L, 1) && !lua_isnumber(L, 2) && lua_type(L, 2) != LUA_TSTRING)
				arg = 2; /* World:GetClosestMarkers(...) */
			/* Overloads: (qty) or (category, qty) — ignore category for now. */
			int qty = 1;
			if (lua_isnumber(L, arg))
				qty = static_cast<int>(lua_tointeger(L, arg));
			else if (lua_isnumber(L, arg + 1))
				qty = static_cast<int>(lua_tointeger(L, arg + 1));
			qty = std::clamp(qty, 1, 64);

			float ax = 0, ay = 0, az = 0;
			if (G::Mumble && G::Mumble->uiTick)
			{
				ax = G::Mumble->fAvatarPosition[0];
				ay = G::Mumble->fAvatarPosition[1];
				az = G::Mumble->fAvatarPosition[2];
			}

			struct Cand { PathingTrails::Marker* m; float d2; };
			std::vector<Cand> cands;
			if (gTickMarkers)
			{
				for (auto& m : *gTickMarkers)
				{
					if (m.luaHidden || m.luaRemoved)
						continue;
					const float dx = m.world.x - ax, dy = m.world.y - ay, dz = m.world.z - az;
					cands.push_back({ &m, dx * dx + dy * dy + dz * dz });
				}
			}
			{
				std::lock_guard<std::mutex> lock(DynMutex());
				for (auto& m : DynMarkers())
				{
					if (m.luaHidden || m.luaRemoved)
						continue;
					const float dx = m.world.x - ax, dy = m.world.y - ay, dz = m.world.z - az;
					cands.push_back({ &m, dx * dx + dy * dy + dz * dz });
				}
			}
			std::sort(cands.begin(), cands.end(),
				[](const Cand& a, const Cand& b) { return a.d2 < b.d2; });

			lua_newtable(L);
			const int n = std::min(qty, static_cast<int>(cands.size()));
			for (int i = 0; i < n; ++i)
			{
				PushMarker(L, cands[static_cast<size_t>(i)].m);
				lua_rawseti(L, -2, i + 1);
			}
			return 1;
		}

		int World_GetClosestMarker(lua_State* L)
		{
			lua_settop(L, 0);
			lua_newtable(L); /* self */
			lua_pushinteger(L, 1);
			World_GetClosestMarkers(L);
			lua_rawgeti(L, -1, 1);
			if (lua_isnil(L, -1))
				return 1;
			return 1;
		}
	}

	void RegisterWorld(lua_State* L)
	{
		lua_newtable(L);
		lua_pushcfunction(L, World_Print); lua_setfield(L, -2, "Print");
		lua_pushcfunction(L, World_MarkerByGuid); lua_setfield(L, -2, "MarkerByGuid");
		lua_pushcfunction(L, World_PathableByGuid); lua_setfield(L, -2, "PathableByGuid");
		lua_pushcfunction(L, World_PathableByGuid); lua_setfield(L, -2, "PathablesByGuid");
		lua_pushcfunction(L, World_CategoryByType); lua_setfield(L, -2, "CategoryByType");
		lua_pushcfunction(L, World_GetClosestMarker); lua_setfield(L, -2, "GetClosestMarker");
		lua_pushcfunction(L, World_GetClosestMarkers); lua_setfield(L, -2, "GetClosestMarkers");
		lua_setglobal(L, "World");
	}
}
