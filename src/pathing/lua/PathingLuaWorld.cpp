#include "PathingLuaInternal.h"

#include "Globals.h"
#include "PathingTrails.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace PathingLuaDetail
{
	namespace
	{
		char gMtCategory[] = "GW2IGH.Category";

		std::string ToLower(std::string s)
		{
			for (char& c : s)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return s;
		}

		bool PrefixMatch(const std::string& typeLow, const std::string& prefixLow)
		{
			if (prefixLow.empty())
				return true;
			if (typeLow == prefixLow)
				return true;
			return typeLow.size() > prefixLow.size() &&
				typeLow.compare(0, prefixLow.size(), prefixLow) == 0 &&
				typeLow[prefixLow.size()] == '.';
		}

		bool CategoryEnabled(const char* ns)
		{
			if (!ns || !ns[0])
				return false;
			const std::string low = ToLower(ns);
			const auto paths = PathingTrails::EnabledPaths();
			for (const std::string& p : paths)
			{
				const std::string el = ToLower(p);
				if (PrefixMatch(low, el) || PrefixMatch(el, low) || low == el)
					return true;
			}
			return false;
		}

		const char* CategoryNs(lua_State* L, int idx)
		{
			if (!lua_istable(L, idx))
				return nullptr;
			lua_getfield(L, idx, "_ns");
			const char* ns = lua_tostring(L, -1);
			lua_pop(L, 1);
			return ns;
		}

		int Category_IsVisible(lua_State* L)
		{
			const char* ns = CategoryNs(L, 1);
			lua_pushboolean(L, CategoryEnabled(ns) ? 1 : 0);
			return 1;
		}

		int Category_Show(lua_State* L)
		{
			const char* ns = CategoryNs(L, 1);
			if (ns && ns[0])
				PathingTrails::SetCategoryEnabled(ns, true);
			return 0;
		}

		int Category_Hide(lua_State* L)
		{
			const char* ns = CategoryNs(L, 1);
			if (ns && ns[0])
				PathingTrails::SetCategoryEnabled(ns, false);
			return 0;
		}

		int Category_GetMarkers(lua_State* L)
		{
			const char* ns = CategoryNs(L, 1);
			const bool getAll = lua_toboolean(L, 2) != 0;
			const std::string prefix = ns ? ToLower(ns) : "";
			lua_newtable(L);
			int n = 0;
			auto consider = [&](PathingTrails::Marker* m) {
				if (!m || m->luaHidden || m->luaRemoved)
					return;
				const std::string low = ToLower(m->label);
				bool match = false;
				if (prefix.empty())
					match = true;
				else if (getAll)
					match = PrefixMatch(low, prefix) || low == prefix;
				else
					match = (low == prefix);
				if (!match)
					return;
				PushMarker(L, m);
				lua_rawseti(L, -2, ++n);
			};
			if (gTickMarkers)
			{
				for (auto& m : *gTickMarkers)
					consider(&m);
			}
			{
				std::lock_guard<std::mutex> lock(DynMutex());
				for (auto& m : DynMarkers())
					consider(&m);
			}
			return 1;
		}

		int Category_GetTrails(lua_State* L)
		{
			const char* ns = CategoryNs(L, 1);
			const bool getAll = lua_toboolean(L, 2) != 0;
			const std::string prefix = ns ? ToLower(ns) : "";
			lua_newtable(L);
			int n = 0;
			if (gTickTrails)
			{
				for (auto& t : *gTickTrails)
				{
					if (t.luaHidden || t.luaRemoved)
						continue;
					const std::string low = ToLower(t.label);
					bool match = false;
					if (prefix.empty())
						match = true;
					else if (getAll)
						match = PrefixMatch(low, prefix) || low == prefix;
					else
						match = (low == prefix);
					if (!match)
						continue;
					PushTrail(L, &t);
					lua_rawseti(L, -2, ++n);
				}
			}
			return 1;
		}

		int Category_Index(lua_State* L)
		{
			const char* ns = CategoryNs(L, 1);
			const char* key = luaL_checkstring(L, 2);
			if (!key)
			{
				lua_pushnil(L);
				return 1;
			}
			if (std::strcmp(key, "Namespace") == 0 || std::strcmp(key, "Name") == 0)
			{
				if (!ns)
					lua_pushstring(L, "");
				else if (std::strcmp(key, "Name") == 0)
				{
					const char* dot = std::strrchr(ns, '.');
					lua_pushstring(L, dot ? dot + 1 : ns);
				}
				else
					lua_pushstring(L, ns);
				return 1;
			}
			if (std::strcmp(key, "Parent") == 0)
			{
				if (!ns || !ns[0])
				{
					lua_pushnil(L);
					return 1;
				}
				const char* dot = std::strrchr(ns, '.');
				if (!dot)
				{
					lua_pushnil(L);
					return 1;
				}
				const std::string parent(ns, static_cast<size_t>(dot - ns));
				PushCategory(L, parent.c_str());
				return 1;
			}
			if (std::strcmp(key, "Root") == 0)
			{
				lua_pushboolean(L, (ns && std::strchr(ns, '.') == nullptr) ? 1 : 0);
				return 1;
			}
			luaL_getmetatable(L, gMtCategory);
			lua_getfield(L, -1, key);
			lua_remove(L, -2);
			return 1;
		}

		int World_Print(lua_State* L)
		{
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
			PushCategory(L, type);
			return 1;
		}

		int World_GetClosestMarkers(lua_State* L)
		{
			int arg = 1;
			if (lua_istable(L, 1) && !lua_isnumber(L, 2) && lua_type(L, 2) != LUA_TSTRING)
				arg = 2;
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
			lua_newtable(L);
			lua_pushinteger(L, 1);
			World_GetClosestMarkers(L);
			lua_rawgeti(L, -1, 1);
			return 1;
		}
	}

	void PushCategory(lua_State* L, const char* ns)
	{
		lua_newtable(L);
		lua_pushstring(L, ns ? ns : "");
		lua_setfield(L, -2, "_ns");
		luaL_getmetatable(L, gMtCategory);
		lua_setmetatable(L, -2);
	}

	void RegisterWorld(lua_State* L)
	{
		luaL_newmetatable(L, gMtCategory);
		lua_pushcfunction(L, Category_Index); lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, Category_IsVisible); lua_setfield(L, -2, "IsVisible");
		lua_pushcfunction(L, Category_Show); lua_setfield(L, -2, "Show");
		lua_pushcfunction(L, Category_Hide); lua_setfield(L, -2, "Hide");
		lua_pushcfunction(L, Category_GetMarkers); lua_setfield(L, -2, "GetMarkers");
		lua_pushcfunction(L, Category_GetTrails); lua_setfield(L, -2, "GetTrails");
		lua_pop(L, 1);

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
