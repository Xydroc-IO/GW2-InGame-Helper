#include "PathingLuaInternal.h"

#include "Globals.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>
#include <wincrypt.h>

namespace PathingLuaDetail
{
	std::vector<PathingTrails::Marker>* gTickMarkers = nullptr;

	namespace
	{
		std::mutex gDynMu;
		std::vector<PathingTrails::Marker> gDyn;
		std::vector<int> gOnTickRefs; /* registry refs */
		char gMtVec[] = "GW2IGH.Vector3";
	}

	std::mutex& DynMutex() { return gDynMu; }
	std::vector<PathingTrails::Marker>& DynMarkers() { return gDyn; }

	void ClearDynMarkers()
	{
		std::lock_guard<std::mutex> lock(gDynMu);
		gDyn.clear();
	}

	void LogLuaError(lua_State* L, const char* ctx)
	{
		const char* err = lua_tostring(L, -1);
		if (G::API && G::API->Log)
		{
			char buf[512]{};
			std::snprintf(buf, sizeof(buf), "PathingLua %s: %s",
				ctx ? ctx : "error", err ? err : "(unknown)");
			G::API->Log(LOGL_WARNING, ADDON_NAME, buf);
		}
	}

	std::string NewGuidBase64()
	{
		unsigned char raw[16]{};
		HCRYPTPROV prov = 0;
		if (CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
		{
			CryptGenRandom(prov, sizeof(raw), raw);
			CryptReleaseContext(prov, 0);
		}
		static const char* k = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		out.reserve(24);
		for (int i = 0; i < 16; i += 3)
		{
			const int b0 = raw[i];
			const int b1 = (i + 1 < 16) ? raw[i + 1] : 0;
			const int b2 = (i + 2 < 16) ? raw[i + 2] : 0;
			out.push_back(k[(b0 >> 2) & 63]);
			out.push_back(k[((b0 & 3) << 4) | ((b1 >> 4) & 15)]);
			out.push_back((i + 1 < 16) ? k[((b1 & 15) << 2) | ((b2 >> 6) & 3)] : '=');
			out.push_back((i + 2 < 16) ? k[b2 & 63] : '=');
		}
		return out;
	}

	PathingTrails::Marker* FindMarkerByGuid(const char* guid)
	{
		if (!guid || !guid[0])
			return nullptr;
		if (gTickMarkers)
		{
			for (auto& m : *gTickMarkers)
			{
				if (std::strcmp(m.guid, guid) == 0)
					return &m;
			}
		}
		std::lock_guard<std::mutex> lock(gDynMu);
		for (auto& m : gDyn)
		{
			if (std::strcmp(m.guid, guid) == 0)
				return &m;
		}
		return nullptr;
	}

	void PushVector3(lua_State* L, float x, float y, float z)
	{
		lua_newtable(L);
		lua_pushnumber(L, x); lua_setfield(L, -2, "X");
		lua_pushnumber(L, y); lua_setfield(L, -2, "Y");
		lua_pushnumber(L, z); lua_setfield(L, -2, "Z");
		luaL_getmetatable(L, gMtVec);
		if (lua_istable(L, -1))
			lua_setmetatable(L, -2);
		else
			lua_pop(L, 1);
	}

	bool ReadVector3(lua_State* L, int idx, float& x, float& y, float& z)
	{
		if (lua_istable(L, idx))
		{
			lua_getfield(L, idx, "X"); x = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
			lua_getfield(L, idx, "Y"); y = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
			lua_getfield(L, idx, "Z"); z = static_cast<float>(lua_tonumber(L, -1)); lua_pop(L, 1);
			return true;
		}
		if (lua_isnumber(L, idx) && lua_isnumber(L, idx + 1) && lua_isnumber(L, idx + 2))
		{
			x = static_cast<float>(lua_tonumber(L, idx));
			y = static_cast<float>(lua_tonumber(L, idx + 1));
			z = static_cast<float>(lua_tonumber(L, idx + 2));
			return true;
		}
		return false;
	}

	static int Vec_Length(lua_State* L)
	{
		float x = 0, y = 0, z = 0;
		ReadVector3(L, 1, x, y, z);
		lua_pushnumber(L, std::sqrt(x * x + y * y + z * z));
		return 1;
	}

	static int Vec_Normalize(lua_State* L)
	{
		float x = 0, y = 0, z = 0;
		ReadVector3(L, 1, x, y, z);
		const float len = std::sqrt(x * x + y * y + z * z);
		if (len > 1e-8f) { x /= len; y /= len; z /= len; }
		lua_pushnumber(L, x); lua_setfield(L, 1, "X");
		lua_pushnumber(L, y); lua_setfield(L, 1, "Y");
		lua_pushnumber(L, z); lua_setfield(L, 1, "Z");
		return 0;
	}

	static int Vec_Dot(lua_State* L)
	{
		float ax = 0, ay = 0, az = 0, bx = 0, by = 0, bz = 0;
		ReadVector3(L, 1, ax, ay, az);
		ReadVector3(L, 2, bx, by, bz);
		lua_pushnumber(L, ax * bx + ay * by + az * bz);
		return 1;
	}

	static int Vec_Cross(lua_State* L)
	{
		float ax = 0, ay = 0, az = 0, bx = 0, by = 0, bz = 0;
		ReadVector3(L, 1, ax, ay, az);
		ReadVector3(L, 2, bx, by, bz);
		PushVector3(L, ay * bz - az * by, az * bx - ax * bz, ax * by - ay * bx);
		return 1;
	}

	static int Vec_New(lua_State* L)
	{
		const float x = static_cast<float>(luaL_optnumber(L, 1, 0));
		const float y = static_cast<float>(luaL_optnumber(L, 2, 0));
		const float z = static_cast<float>(luaL_optnumber(L, 3, 0));
		PushVector3(L, x, y, z);
		return 1;
	}

	void RegisterTypes(lua_State* L)
	{
		luaL_newmetatable(L, gMtVec);
		lua_pushcfunction(L, Vec_Length); lua_setfield(L, -2, "Length");
		lua_pushcfunction(L, Vec_Normalize); lua_setfield(L, -2, "Normalize");
		lua_pushcfunction(L, Vec_Dot); lua_setfield(L, -2, "Dot");
		lua_pushcfunction(L, Vec_Cross); lua_setfield(L, -2, "Cross");
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
		lua_pop(L, 1);

		lua_newtable(L);
		lua_pushcfunction(L, Vec_New);
		lua_setfield(L, -2, "new");
		lua_setglobal(L, "Vector3");

		/* Color helper: Color(a,r,g,b) or from ARGB int */
		lua_newtable(L);
		lua_pushcfunction(L, [](lua_State* L) -> int {
			if (lua_gettop(L) >= 4)
			{
				const int a = static_cast<int>(lua_tointeger(L, 1));
				const int r = static_cast<int>(lua_tointeger(L, 2));
				const int g = static_cast<int>(lua_tointeger(L, 3));
				const int b = static_cast<int>(lua_tointeger(L, 4));
				lua_pushinteger(L, ((a & 255) << 24) | ((r & 255) << 16) |
					((g & 255) << 8) | (b & 255));
			}
			else
				lua_pushinteger(L, luaL_optinteger(L, 1, 0xFFFFFFFFu));
			return 1;
		});
		lua_setfield(L, -2, "FromArgb");
		lua_setglobal(L, "Color");
	}

	void ClearOnTick()
	{
		gOnTickRefs.clear();
	}

	int Event_OnTick(lua_State* L)
	{
		/* Event:OnTick(fn) or Event.OnTick(fn) - last arg is function */
		int fnIdx = lua_gettop(L);
		if (!lua_isfunction(L, fnIdx))
			return luaL_error(L, "Event:OnTick expects a function");
		lua_pushvalue(L, fnIdx);
		const int ref = luaL_ref(L, LUA_REGISTRYINDEX);
		gOnTickRefs.push_back(ref);
		return 0;
	}

	void RunOnTick(lua_State* L, float elapsedMs)
	{
		if (!L || gOnTickRefs.empty())
			return;
		for (int ref : gOnTickRefs)
		{
			lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
			if (!lua_isfunction(L, -1))
			{
				lua_pop(L, 1);
				continue;
			}
			lua_newtable(L);
			lua_pushnumber(L, elapsedMs);
			lua_setfield(L, -2, "ElapsedGameTime");
			lua_pushnumber(L, elapsedMs);
			lua_setfield(L, -2, "TotalGameTime");
			if (lua_pcall(L, 1, 0, 0) != LUA_OK)
			{
				LogLuaError(L, "Event:OnTick");
				lua_pop(L, 1);
			}
		}
	}

	void RegisterEvent(lua_State* L)
	{
		lua_newtable(L);
		lua_pushcfunction(L, Event_OnTick);
		lua_setfield(L, -2, "OnTick");
		lua_setglobal(L, "Event");
	}
}
