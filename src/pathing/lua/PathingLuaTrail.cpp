#include "PathingLuaInternal.h"

#include "Globals.h"
#include "PathingIndex.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace PathingLuaDetail
{
	std::vector<PathingTrails::Trail>* gTickTrails = nullptr;

	namespace
	{
		char gMtTrail[] = "GW2IGH.Trail";

		PathingTrails::Trail* FindTrailByGuid(const char* guid)
		{
			if (!guid || !guid[0])
				return nullptr;
			auto match = [&](PathingTrails::Trail& t) -> bool {
				if (t.luaRemoved)
					return false;
				if (t.guid[0] && std::strcmp(t.guid, guid) == 0)
					return true;
				/* Packs often omit trail GUID — accept type/label. */
				return t.label[0] && std::strcmp(t.label, guid) == 0;
			};
			if (gTickTrails)
			{
				for (auto& t : *gTickTrails)
					if (match(t))
						return &t;
				return nullptr;
			}
			std::unique_lock<std::mutex> lock(PathingDetail::gMutex, std::try_to_lock);
			if (!lock.owns_lock())
				return nullptr;
			for (auto& t : PathingDetail::gCurrentAll)
				if (match(t))
					return &t;
			return nullptr;
		}

		int Trail_Index(lua_State* L)
		{
			PathingTrails::Trail* t = CheckTrail(L, 1);
			const char* key = luaL_checkstring(L, 2);
			if (!t || !key)
			{
				lua_pushnil(L);
				return 1;
			}
			if (std::strcmp(key, "Guid") == 0)
			{
				lua_pushstring(L, t->guid[0] ? t->guid : t->label);
				return 1;
			}
			if (std::strcmp(key, "Type") == 0 || std::strcmp(key, "Category") == 0)
			{ lua_pushstring(L, t->label); return 1; }
			if (std::strcmp(key, "MapId") == 0) { lua_pushinteger(L, t->mapId); return 1; }
			if (std::strcmp(key, "Alpha") == 0) { lua_pushnumber(L, t->alpha); return 1; }
			if (std::strcmp(key, "Tint") == 0) { lua_pushinteger(L, t->color); return 1; }
			if (std::strcmp(key, "TrailScale") == 0)
			{ lua_pushnumber(L, t->trailScale); return 1; }
			if (std::strcmp(key, "AnimSpeed") == 0)
			{ lua_pushnumber(L, t->animSpeed); return 1; }
			if (std::strcmp(key, "FadeNear") == 0) { lua_pushnumber(L, t->fadeNear); return 1; }
			if (std::strcmp(key, "FadeFar") == 0) { lua_pushnumber(L, t->fadeFar); return 1; }
			luaL_getmetatable(L, gMtTrail);
			lua_getfield(L, -1, key);
			lua_remove(L, -2);
			return 1;
		}

		int Trail_NewIndex(lua_State* L)
		{
			PathingTrails::Trail* t = CheckTrail(L, 1);
			const char* key = luaL_checkstring(L, 2);
			if (!t || !key)
				return 0;
			if (std::strcmp(key, "Alpha") == 0)
				t->alpha = static_cast<float>(lua_tonumber(L, 3));
			else if (std::strcmp(key, "Tint") == 0)
				t->color = static_cast<uint32_t>(lua_tointeger(L, 3));
			else if (std::strcmp(key, "TrailScale") == 0)
				t->trailScale = static_cast<float>(lua_tonumber(L, 3));
			else if (std::strcmp(key, "AnimSpeed") == 0)
				t->animSpeed = static_cast<float>(lua_tonumber(L, 3));
			else if (std::strcmp(key, "FadeNear") == 0)
				t->fadeNear = static_cast<float>(lua_tonumber(L, 3));
			else if (std::strcmp(key, "FadeFar") == 0)
				t->fadeFar = static_cast<float>(lua_tonumber(L, 3));
			return 0;
		}

		int Trail_Remove(lua_State* L)
		{
			PathingTrails::Trail* t = CheckTrail(L, 1);
			if (t)
			{
				t->luaRemoved = true;
				t->luaHidden = true;
			}
			return 0;
		}

		int Trail_SetTexture(lua_State* L)
		{
			PathingTrails::Trail* t = CheckTrail(L, 1);
			if (!t)
				return 0;
			if (lua_isnumber(L, 2))
			{
				RequestCdnTexture(static_cast<int>(lua_tointeger(L, 2)),
					t->textureId, sizeof(t->textureId));
				return 0;
			}
			const char* path = luaL_optstring(L, 2, "");
			if (path && path[0])
				std::snprintf(t->textureId, sizeof(t->textureId), "%s", path);
			return 0;
		}

		int Trail_GetBehavior(lua_State* L)
		{
			(void)CheckTrail(L, 1);
			lua_pushnil(L); /* trails rarely carry TacO behaviors */
			return 1;
		}

		int World_TrailByGuid(lua_State* L)
		{
			const int arg = lua_istable(L, 1) ? 2 : 1;
			const char* guid = luaL_checkstring(L, arg);
			PathingTrails::Trail* t = FindTrailByGuid(guid);
			if (!t)
			{
				lua_pushnil(L);
				return 1;
			}
			PushTrail(L, t);
			return 1;
		}

		int World_GetClosestTrails(lua_State* L)
		{
			int arg = 1;
			if (lua_istable(L, 1) && !lua_isnumber(L, 2))
				arg = 2;
			int qty = 1;
			if (lua_isnumber(L, arg))
				qty = static_cast<int>(lua_tointeger(L, arg));
			else if (lua_isnumber(L, arg + 1))
				qty = static_cast<int>(lua_tointeger(L, arg + 1));
			qty = std::clamp(qty, 1, 32);

			float ax = 0, ay = 0, az = 0;
			if (G::Mumble && G::Mumble->uiTick)
			{
				ax = G::Mumble->fAvatarPosition[0];
				ay = G::Mumble->fAvatarPosition[1];
				az = G::Mumble->fAvatarPosition[2];
			}

			struct Cand { PathingTrails::Trail* t; float d2; };
			std::vector<Cand> cands;

			auto consider = [&](PathingTrails::Trail& t) {
				if (t.luaHidden || t.luaRemoved || t.worldPoints.size() < 2)
					return;
				float best = 1e30f;
				for (const auto& p : t.worldPoints)
				{
					if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
						continue;
					const float dx = p.x - ax, dy = p.y - ay, dz = p.z - az;
					const float d2 = dx * dx + dy * dy + dz * dz;
					if (d2 < best)
						best = d2;
				}
				if (best < 1e29f)
					cands.push_back({ &t, best });
			};

			if (gTickTrails)
			{
				for (auto& t : *gTickTrails)
					consider(t);
			}
			else
			{
				std::unique_lock<std::mutex> lock(PathingDetail::gMutex, std::try_to_lock);
				if (lock.owns_lock())
				{
					for (auto& t : PathingDetail::gCurrentAll)
						consider(t);
				}
			}

			std::sort(cands.begin(), cands.end(),
				[](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
			lua_newtable(L);
			const int n = std::min(qty, static_cast<int>(cands.size()));
			for (int i = 0; i < n; ++i)
			{
				PushTrail(L, cands[static_cast<size_t>(i)].t);
				lua_rawseti(L, -2, i + 1);
			}
			return 1;
		}

		int World_GetClosestTrail(lua_State* L)
		{
			lua_settop(L, 0);
			lua_newtable(L);
			lua_pushinteger(L, 1);
			World_GetClosestTrails(L);
			lua_rawgeti(L, -1, 1);
			return 1;
		}
	}

	void PushTrail(lua_State* L, PathingTrails::Trail* t)
	{
		lua_newtable(L);
		lua_pushlightuserdata(L, t);
		lua_setfield(L, -2, "_ptr");
		luaL_getmetatable(L, gMtTrail);
		lua_setmetatable(L, -2);
	}

	PathingTrails::Trail* CheckTrail(lua_State* L, int idx)
	{
		if (!lua_istable(L, idx))
			return nullptr;
		lua_getfield(L, idx, "_ptr");
		auto* t = static_cast<PathingTrails::Trail*>(lua_touserdata(L, -1));
		lua_pop(L, 1);
		return t;
	}

	void RegisterTrail(lua_State* L)
	{
		luaL_newmetatable(L, gMtTrail);
		lua_pushcfunction(L, Trail_Index); lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, Trail_NewIndex); lua_setfield(L, -2, "__newindex");
		lua_pushcfunction(L, Trail_Remove); lua_setfield(L, -2, "Remove");
		lua_pushcfunction(L, Trail_SetTexture); lua_setfield(L, -2, "SetTexture");
		lua_pushcfunction(L, Trail_GetBehavior); lua_setfield(L, -2, "GetBehavior");
		lua_pop(L, 1);
	}

	void RegisterWorldTrail(lua_State* L)
	{
		lua_getglobal(L, "World");
		if (!lua_istable(L, -1))
		{
			lua_pop(L, 1);
			return;
		}
		lua_pushcfunction(L, World_TrailByGuid); lua_setfield(L, -2, "TrailByGuid");
		lua_pushcfunction(L, World_GetClosestTrail); lua_setfield(L, -2, "GetClosestTrail");
		lua_pushcfunction(L, World_GetClosestTrails); lua_setfield(L, -2, "GetClosestTrails");
		lua_pop(L, 1);
	}
}
