#include "PathingLuaInternal.h"

#include "Globals.h"
#include "PathingTrails.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace PathingLuaDetail
{
	namespace
	{
		int Pack_SetCategoryEnabled(lua_State* L)
		{
			const int base = lua_istable(L, 1) ? 2 : 1;
			const char* path = luaL_checkstring(L, base);
			const bool on = lua_toboolean(L, base + 1) != 0;
			if (path && path[0])
				PathingTrails::SetCategoryEnabled(path, on);
			return 0;
		}

		int Pack_Require(lua_State* L)
		{
			const int arg = lua_istable(L, 1) ? 2 : 1;
			const char* path = luaL_optstring(L, arg, "");
			if (!RequireScript(L, path ? path : ""))
			{
				/* Soft-fail - Blish packs often Require optional helpers. */
				if (G::API && G::API->Log && path && path[0])
				{
					char buf[256];
					std::snprintf(buf, sizeof(buf), "PathingLua: Pack:Require missing '%s'", path);
					G::API->Log(LOGL_WARNING, ADDON_NAME, buf);
				}
			}
			return 0;
		}

		int Pack_CreateMarker(lua_State* L)
		{
			const int attrIdx = lua_istable(L, 1) && lua_istable(L, 2) ? 2
				: (lua_istable(L, 1) ? 1 : 0);

			PathingTrails::Marker m{};
			m.luaDynamic = true;
			m.mapId = 0;
			if (G::Mumble && G::Mumble->uiTick)
			{
				const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
				if (ctx)
					m.mapId = ctx->mapId;
				m.world = {
					G::Mumble->fAvatarPosition[0],
					G::Mumble->fAvatarPosition[1],
					G::Mumble->fAvatarPosition[2]
				};
			}
			const std::string guid = NewGuidBase64();
			std::snprintf(m.guid, sizeof(m.guid), "%s", guid.c_str());
			std::snprintf(m.label, sizeof(m.label), "dynamic");
			m.color = 0xFFFFC828u;
			m.iconSize = 1.f;
			m.heightOffset = 1.5f;
			m.minimapVisible = true;
			m.inGameVisible = true;

			if (attrIdx && lua_istable(L, attrIdx))
			{
				lua_getfield(L, attrIdx, "xpos");
				if (lua_isnumber(L, -1)) m.world.x = static_cast<float>(lua_tonumber(L, -1));
				lua_pop(L, 1);
				lua_getfield(L, attrIdx, "ypos");
				if (lua_isnumber(L, -1)) m.world.y = static_cast<float>(lua_tonumber(L, -1));
				lua_pop(L, 1);
				lua_getfield(L, attrIdx, "zpos");
				if (lua_isnumber(L, -1)) m.world.z = static_cast<float>(lua_tonumber(L, -1));
				lua_pop(L, 1);
				lua_getfield(L, attrIdx, "type");
				if (lua_isstring(L, -1))
					std::snprintf(m.label, sizeof(m.label), "%s", lua_tostring(L, -1));
				lua_pop(L, 1);
				lua_getfield(L, attrIdx, "GUID");
				if (lua_isstring(L, -1))
					std::snprintf(m.guid, sizeof(m.guid), "%s", lua_tostring(L, -1));
				lua_pop(L, 1);
				lua_getfield(L, attrIdx, "MapID");
				if (lua_isnumber(L, -1))
					m.mapId = static_cast<uint32_t>(lua_tointeger(L, -1));
				lua_pop(L, 1);
				lua_getfield(L, attrIdx, "color");
				if (lua_isnumber(L, -1))
					m.color = static_cast<uint32_t>(lua_tointeger(L, -1));
				lua_pop(L, 1);
				lua_getfield(L, attrIdx, "iconSize");
				if (lua_isnumber(L, -1))
					m.iconSize = static_cast<float>(lua_tonumber(L, -1));
				lua_pop(L, 1);
				lua_getfield(L, attrIdx, "TriggerRange");
				if (lua_isnumber(L, -1))
					m.triggerRange = static_cast<float>(lua_tonumber(L, -1));
				lua_pop(L, 1);
				lua_getfield(L, attrIdx, "AutoTrigger");
				if (lua_isboolean(L, -1))
					m.autoTrigger = lua_toboolean(L, -1) != 0;
				lua_pop(L, 1);
				lua_getfield(L, attrIdx, "Info");
				if (lua_isstring(L, -1))
					std::snprintf(m.info, sizeof(m.info), "%s", lua_tostring(L, -1));
				lua_pop(L, 1);
			}

			PathingTrails::Marker* live = nullptr;
			{
				std::lock_guard<std::mutex> lock(DynMutex());
				DynMarkers().push_back(std::move(m));
				live = &DynMarkers().back();
			}
			PushMarker(L, live);
			return 1;
		}
	}

	void RegisterPack(lua_State* L)
	{
		lua_newtable(L);
		lua_pushcfunction(L, Pack_SetCategoryEnabled);
		lua_setfield(L, -2, "SetCategoryEnabled");
		lua_pushcfunction(L, Pack_Require);
		lua_setfield(L, -2, "Require");
		lua_pushcfunction(L, Pack_CreateMarker);
		lua_setfield(L, -2, "CreateMarker");
		lua_setglobal(L, "Pack");
	}
}
