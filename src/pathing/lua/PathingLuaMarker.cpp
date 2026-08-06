#include "PathingLuaInternal.h"

#include "Globals.h"
#include "MarkerBehaviors.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace PathingLuaDetail
{
	namespace
	{
		char gMtMarker[] = "GW2IGH.Marker";

		int Marker_Index(lua_State* L)
		{
			PathingTrails::Marker* m = CheckMarker(L, 1);
			const char* key = luaL_checkstring(L, 2);
			if (!m || !key)
			{
				lua_pushnil(L);
				return 1;
			}
			if (std::strcmp(key, "Guid") == 0) { lua_pushstring(L, m->guid); return 1; }
			if (std::strcmp(key, "Type") == 0 || std::strcmp(key, "Category") == 0)
			{ lua_pushstring(L, m->label); return 1; }
			if (std::strcmp(key, "MapId") == 0) { lua_pushinteger(L, m->mapId); return 1; }
			if (std::strcmp(key, "Alpha") == 0) { lua_pushnumber(L, m->alpha); return 1; }
			if (std::strcmp(key, "Tint") == 0) { lua_pushinteger(L, m->color); return 1; }
			if (std::strcmp(key, "TriggerRange") == 0)
			{ lua_pushnumber(L, m->triggerRange); return 1; }
			if (std::strcmp(key, "AutoTrigger") == 0)
			{ lua_pushboolean(L, m->autoTrigger); return 1; }
			if (std::strcmp(key, "TipName") == 0) { lua_pushstring(L, m->tipName); return 1; }
			if (std::strcmp(key, "TipDescription") == 0)
			{ lua_pushstring(L, m->tipDescription); return 1; }
			if (std::strcmp(key, "HeightOffset") == 0)
			{ lua_pushnumber(L, m->heightOffset); return 1; }
			if (std::strcmp(key, "Size") == 0 || std::strcmp(key, "IconSize") == 0)
			{ lua_pushnumber(L, m->iconSize); return 1; }
			if (std::strcmp(key, "FadeNear") == 0) { lua_pushnumber(L, m->fadeNear); return 1; }
			if (std::strcmp(key, "FadeFar") == 0) { lua_pushnumber(L, m->fadeFar); return 1; }
			if (std::strcmp(key, "Focused") == 0) { lua_pushboolean(L, 0); return 1; }
			if (std::strcmp(key, "Texture") == 0) { lua_pushstring(L, m->iconId); return 1; }
			if (std::strcmp(key, "Category") == 0)
			{
				PushCategory(L, m->label);
				return 1;
			}
			if (std::strcmp(key, "Position") == 0)
			{
				PushVector3(L, m->world.x, m->world.y, m->world.z);
				return 1;
			}
			if (std::strcmp(key, "DistanceToPlayer") == 0)
			{
				float ax = 0, ay = 0, az = 0;
				if (G::Mumble && G::Mumble->uiTick)
				{
					ax = G::Mumble->fAvatarPosition[0];
					ay = G::Mumble->fAvatarPosition[1];
					az = G::Mumble->fAvatarPosition[2];
				}
				const float dx = m->world.x - ax, dy = m->world.y - ay, dz = m->world.z - az;
				lua_pushnumber(L, std::sqrt(dx * dx + dy * dy + dz * dz));
				return 1;
			}
			/* Fall through to method table. */
			luaL_getmetatable(L, gMtMarker);
			lua_getfield(L, -1, key);
			lua_remove(L, -2);
			return 1;
		}

		int Marker_NewIndex(lua_State* L)
		{
			PathingTrails::Marker* m = CheckMarker(L, 1);
			const char* key = luaL_checkstring(L, 2);
			if (!m || !key)
				return 0;
			if (std::strcmp(key, "Alpha") == 0)
				m->alpha = static_cast<float>(lua_tonumber(L, 3));
			else if (std::strcmp(key, "Tint") == 0)
				m->color = static_cast<uint32_t>(lua_tointeger(L, 3));
			else if (std::strcmp(key, "TriggerRange") == 0)
				m->triggerRange = static_cast<float>(lua_tonumber(L, 3));
			else if (std::strcmp(key, "AutoTrigger") == 0)
				m->autoTrigger = lua_toboolean(L, 3) != 0;
			else if (std::strcmp(key, "TipName") == 0)
				std::snprintf(m->tipName, sizeof(m->tipName), "%s", luaL_optstring(L, 3, ""));
			else if (std::strcmp(key, "TipDescription") == 0)
				std::snprintf(m->tipDescription, sizeof(m->tipDescription), "%s",
					luaL_optstring(L, 3, ""));
			else if (std::strcmp(key, "HeightOffset") == 0)
				m->heightOffset = static_cast<float>(lua_tonumber(L, 3));
			else if (std::strcmp(key, "Size") == 0 || std::strcmp(key, "IconSize") == 0)
				m->iconSize = static_cast<float>(lua_tonumber(L, 3));
			else if (std::strcmp(key, "Texture") == 0)
				std::snprintf(m->iconId, sizeof(m->iconId), "%s", luaL_optstring(L, 3, ""));
			else if (std::strcmp(key, "Position") == 0)
			{
				float x = 0, y = 0, z = 0;
				if (ReadVector3(L, 3, x, y, z))
					m->world = { x, y, z };
			}
			return 0;
		}

		int Marker_SetPos(lua_State* L)
		{
			PathingTrails::Marker* m = CheckMarker(L, 1);
			if (!m)
				return 0;
			float x = 0, y = 0, z = 0;
			if (lua_istable(L, 2))
				ReadVector3(L, 2, x, y, z);
			else
			{
				x = static_cast<float>(luaL_checknumber(L, 2));
				y = static_cast<float>(luaL_checknumber(L, 3));
				z = static_cast<float>(luaL_checknumber(L, 4));
			}
			m->world = { x, y, z };
			return 0;
		}

		int Marker_SetPosX(lua_State* L)
		{
			PathingTrails::Marker* m = CheckMarker(L, 1);
			if (m) m->world.x = static_cast<float>(luaL_checknumber(L, 2));
			return 0;
		}
		int Marker_SetPosY(lua_State* L)
		{
			PathingTrails::Marker* m = CheckMarker(L, 1);
			if (m) m->world.y = static_cast<float>(luaL_checknumber(L, 2));
			return 0;
		}
		int Marker_SetPosZ(lua_State* L)
		{
			PathingTrails::Marker* m = CheckMarker(L, 1);
			if (m) m->world.z = static_cast<float>(luaL_checknumber(L, 2));
			return 0;
		}

		int Marker_SetRot(lua_State* L) { (void)L; return 0; } /* visual stub */
		int Marker_SetRotX(lua_State* L) { (void)L; return 0; }
		int Marker_SetRotY(lua_State* L) { (void)L; return 0; }
		int Marker_SetRotZ(lua_State* L) { (void)L; return 0; }

		int Marker_Remove(lua_State* L)
		{
			PathingTrails::Marker* m = CheckMarker(L, 1);
			if (m)
			{
				m->luaRemoved = true;
				m->luaHidden = true;
			}
			return 0;
		}

		int Marker_SetTexture(lua_State* L)
		{
			PathingTrails::Marker* m = CheckMarker(L, 1);
			if (!m)
				return 0;
			if (lua_isnumber(L, 2))
			{
				RequestCdnTexture(static_cast<int>(lua_tointeger(L, 2)),
					m->iconId, sizeof(m->iconId));
				return 0;
			}
			const char* path = luaL_optstring(L, 2, "");
			if (path && path[0])
				std::snprintf(m->iconId, sizeof(m->iconId), "%s", path);
			return 0;
		}

		int Marker_Focus(lua_State* L) { (void)CheckMarker(L, 1); return 0; }
		int Marker_Unfocus(lua_State* L) { (void)CheckMarker(L, 1); return 0; }
		int Marker_Interact(lua_State* L)
		{
			(void)CheckMarker(L, 1);
			MarkerBehaviors::RequestInteract();
			return 0;
		}

		int Marker_GetBehavior(lua_State* L)
		{
			PathingTrails::Marker* m = CheckMarker(L, 1);
			if (!m)
			{
				lua_pushnil(L);
				return 1;
			}
			PushBehavior(L, m);
			return 1;
		}

		int Marker_Hide(lua_State* L)
		{
			PathingTrails::Marker* m = CheckMarker(L, 1);
			if (m)
				m->luaHidden = true;
			return 0;
		}

		int Marker_Show(lua_State* L)
		{
			PathingTrails::Marker* m = CheckMarker(L, 1);
			if (m)
				m->luaHidden = false;
			return 0;
		}

		int Marker_IsVisible(lua_State* L)
		{
			PathingTrails::Marker* m = CheckMarker(L, 1);
			lua_pushboolean(L, (m && !m->luaHidden && !m->luaRemoved) ? 1 : 0);
			return 1;
		}
	}

	namespace
	{
		const char* BehaviorName(int b)
		{
			switch (b)
			{
			case 1: return "ReappearOnMapChange";
			case 2: return "ReappearOnDailyReset";
			case 3: return "OnlyWhileAttributeIsActive";
			case 4: return "ReappearOnWeeklyReset";
			case 5: return "ReappearOnInstanceReset";
			case 6: return "OncePerCharacter";
			case 7: return "OncePerAccount";
			case 101: return "ReappearAfterTimer";
			default: return "Default";
			}
		}

		int Behavior_Reset(lua_State* L)
		{
			(void)L;
			return 0;
		}
	}

	void PushBehavior(lua_State* L, const PathingTrails::Marker* m)
	{
		if (!m)
		{
			lua_pushnil(L);
			return;
		}
		lua_newtable(L);
		lua_pushstring(L, "Behavior");
		lua_setfield(L, -2, "Type");
		lua_pushinteger(L, m->behavior);
		lua_setfield(L, -2, "BehaviorType");
		lua_pushstring(L, BehaviorName(m->behavior));
		lua_setfield(L, -2, "Name");
		lua_pushboolean(L, m->invertBehavior ? 1 : 0);
		lua_setfield(L, -2, "InvertBehavior");
		lua_pushnumber(L, m->resetLength);
		lua_setfield(L, -2, "ResetLength");
		lua_pushcfunction(L, Behavior_Reset);
		lua_setfield(L, -2, "Reset");
	}

	void PushMarker(lua_State* L, PathingTrails::Marker* m)
	{
		lua_newtable(L);
		lua_pushlightuserdata(L, m);
		lua_setfield(L, -2, "_ptr");
		luaL_getmetatable(L, gMtMarker);
		lua_setmetatable(L, -2);
	}

	PathingTrails::Marker* CheckMarker(lua_State* L, int idx)
	{
		if (!lua_istable(L, idx))
			return nullptr;
		lua_getfield(L, idx, "_ptr");
		auto* m = static_cast<PathingTrails::Marker*>(lua_touserdata(L, -1));
		lua_pop(L, 1);
		return m;
	}

	void RegisterMarker(lua_State* L)
	{
		luaL_newmetatable(L, gMtMarker);
		lua_pushcfunction(L, Marker_Index); lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, Marker_NewIndex); lua_setfield(L, -2, "__newindex");
		lua_pushcfunction(L, Marker_SetPos); lua_setfield(L, -2, "SetPos");
		lua_pushcfunction(L, Marker_SetPosX); lua_setfield(L, -2, "SetPosX");
		lua_pushcfunction(L, Marker_SetPosY); lua_setfield(L, -2, "SetPosY");
		lua_pushcfunction(L, Marker_SetPosZ); lua_setfield(L, -2, "SetPosZ");
		lua_pushcfunction(L, Marker_SetRot); lua_setfield(L, -2, "SetRot");
		lua_pushcfunction(L, Marker_SetRotX); lua_setfield(L, -2, "SetRotX");
		lua_pushcfunction(L, Marker_SetRotY); lua_setfield(L, -2, "SetRotY");
		lua_pushcfunction(L, Marker_SetRotZ); lua_setfield(L, -2, "SetRotZ");
		lua_pushcfunction(L, Marker_Remove); lua_setfield(L, -2, "Remove");
		lua_pushcfunction(L, Marker_SetTexture); lua_setfield(L, -2, "SetTexture");
		lua_pushcfunction(L, Marker_Focus); lua_setfield(L, -2, "Focus");
		lua_pushcfunction(L, Marker_Unfocus); lua_setfield(L, -2, "Unfocus");
		lua_pushcfunction(L, Marker_Interact); lua_setfield(L, -2, "Interact");
		lua_pushcfunction(L, Marker_GetBehavior); lua_setfield(L, -2, "GetBehavior");
		lua_pushcfunction(L, Marker_Hide); lua_setfield(L, -2, "Hide");
		lua_pushcfunction(L, Marker_Show); lua_setfield(L, -2, "Show");
		lua_pushcfunction(L, Marker_IsVisible); lua_setfield(L, -2, "IsVisible");
		lua_pop(L, 1);
	}
}
