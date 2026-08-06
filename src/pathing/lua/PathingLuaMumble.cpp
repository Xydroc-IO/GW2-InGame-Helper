#include "PathingLuaInternal.h"

#include "Globals.h"
#include "MumbleIdentity.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <cstring>

namespace PathingLuaDetail
{
	namespace
	{
		int Mumble_PlayerPosition(lua_State* L)
		{
			float x = 0, y = 0, z = 0;
			if (G::Mumble && G::Mumble->uiTick)
			{
				x = G::Mumble->fAvatarPosition[0];
				y = G::Mumble->fAvatarPosition[1];
				z = G::Mumble->fAvatarPosition[2];
			}
			lua_pushnumber(L, x);
			lua_pushnumber(L, y);
			lua_pushnumber(L, z);
			return 3;
		}

		void PushPlayerCharacter(lua_State* L)
		{
			lua_newtable(L);
			float x = 0, y = 0, z = 0;
			if (G::Mumble && G::Mumble->uiTick)
			{
				x = G::Mumble->fAvatarPosition[0];
				y = G::Mumble->fAvatarPosition[1];
				z = G::Mumble->fAvatarPosition[2];
			}
			PushVector3(L, x, y, z);
			lua_setfield(L, -2, "Position");
			PushVector3(L,
				G::Mumble ? G::Mumble->fAvatarFront[0] : 0.f,
				G::Mumble ? G::Mumble->fAvatarFront[1] : 0.f,
				G::Mumble ? G::Mumble->fAvatarFront[2] : 0.f);
			lua_setfield(L, -2, "Forward");
			lua_pushstring(L, MumbleIdentity::CharacterName());
			lua_setfield(L, -2, "Name");
			int mount = 0;
			if (G::Mumble && G::Mumble->uiTick)
			{
				const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
				if (ctx)
					mount = static_cast<int>(ctx->mountIndex);
			}
			lua_pushinteger(L, mount);
			lua_setfield(L, -2, "CurrentMount");
		}

		void PushPlayerCamera(lua_State* L)
		{
			lua_newtable(L);
			PushVector3(L,
				G::Mumble ? G::Mumble->fCameraPosition[0] : 0.f,
				G::Mumble ? G::Mumble->fCameraPosition[1] : 0.f,
				G::Mumble ? G::Mumble->fCameraPosition[2] : 0.f);
			lua_setfield(L, -2, "Position");
			PushVector3(L,
				G::Mumble ? G::Mumble->fCameraFront[0] : 0.f,
				G::Mumble ? G::Mumble->fCameraFront[1] : 0.f,
				G::Mumble ? G::Mumble->fCameraFront[2] : 0.f);
			lua_setfield(L, -2, "Forward");
		}

		void PushCurrentMap(lua_State* L)
		{
			lua_newtable(L);
			uint32_t mapId = 0;
			uint32_t mapType = 0;
			if (G::Mumble && G::Mumble->uiTick)
			{
				const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
				if (ctx)
				{
					mapId = ctx->mapId;
					mapType = ctx->mapType;
				}
			}
			lua_pushinteger(L, mapId);
			lua_setfield(L, -2, "Id");
			lua_pushinteger(L, mapType);
			lua_setfield(L, -2, "Type");
			lua_pushboolean(L, mapType == 2 || mapType == 3 || mapType == 9);
			lua_setfield(L, -2, "IsCompetitiveMode");
		}

		int Mumble_Index(lua_State* L)
		{
			const char* key = luaL_checkstring(L, 2);
			if (std::strcmp(key, "PlayerPosition") == 0)
			{
				lua_pushcfunction(L, Mumble_PlayerPosition);
				return 1;
			}
			if (std::strcmp(key, "PlayerCharacter") == 0)
			{
				PushPlayerCharacter(L);
				return 1;
			}
			if (std::strcmp(key, "PlayerCamera") == 0)
			{
				PushPlayerCamera(L);
				return 1;
			}
			if (std::strcmp(key, "CurrentMap") == 0)
			{
				PushCurrentMap(L);
				return 1;
			}
			if (std::strcmp(key, "Info") == 0)
			{
				lua_newtable(L);
				uint32_t build = 0;
				if (G::Mumble && G::Mumble->uiTick)
				{
					const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
					if (ctx) build = ctx->buildId;
				}
				lua_pushinteger(L, build);
				lua_setfield(L, -2, "BuildId");
				lua_pushboolean(L, G::NexusLink ? G::NexusLink->IsGameplay : 0);
				lua_setfield(L, -2, "IsGameFocused");
				return 1;
			}
			if (std::strcmp(key, "IsAvailable") == 0)
			{
				lua_pushboolean(L, (G::Mumble && G::Mumble->uiTick) ? 1 : 0);
				return 1;
			}
			lua_pushnil(L);
			return 1;
		}
	}

	void RegisterMumble(lua_State* L)
	{
		lua_newtable(L);
		lua_newtable(L);
		lua_pushcfunction(L, Mumble_Index);
		lua_setfield(L, -2, "__index");
		lua_setmetatable(L, -2);
		lua_pushcfunction(L, Mumble_PlayerPosition);
		lua_setfield(L, -2, "PlayerPosition");
		lua_setglobal(L, "Mumble");
	}
}
