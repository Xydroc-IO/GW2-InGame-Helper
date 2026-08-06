#include "PathingLuaInternal.h"

#include "Globals.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <cstring>
#include <string>
#include <unordered_map>

#include <windows.h>

namespace PathingLuaDetail
{
	namespace
	{
		std::unordered_map<std::string, std::string> gWatch;

		void LogMsg(ELogLevel level, const char* msg)
		{
			if (G::API && G::API->Log)
				G::API->Log(level, ADDON_NAME, msg ? msg : "");
		}

		int Debug_Msg(lua_State* L, ELogLevel level)
		{
			int arg = lua_istable(L, 1) ? 2 : 1;
			const char* s = luaL_optstring(L, arg, "");
			LogMsg(level, s);
			return 0;
		}

		int Debug_Print(lua_State* L) { return Debug_Msg(L, LOGL_INFO); }
		int Debug_Info(lua_State* L) { return Debug_Msg(L, LOGL_INFO); }
		int Debug_Warn(lua_State* L) { return Debug_Msg(L, LOGL_WARNING); }
		int Debug_Error(lua_State* L) { return Debug_Msg(L, LOGL_WARNING); }

		int Debug_ShowMessage(lua_State* L)
		{
			return Debug_Msg(L, LOGL_INFO);
		}

		int Debug_Watch(lua_State* L)
		{
			int arg = lua_istable(L, 1) ? 2 : 1;
			const char* key = luaL_optstring(L, arg, "");
			if (!key || !key[0])
				return 0;
			std::string val = "(table)";
			if (lua_isstring(L, arg + 1) || lua_isnumber(L, arg + 1))
				val = luaL_optstring(L, arg + 1, "");
			else if (lua_isboolean(L, arg + 1))
				val = lua_toboolean(L, arg + 1) ? "true" : "false";
			else if (lua_isnil(L, arg + 1))
				val = "nil";
			gWatch[key] = val;
			return 0;
		}

		int Debug_ClearWatch(lua_State* L)
		{
			int arg = lua_istable(L, 1) ? 2 : 1;
			if (lua_isnoneornil(L, arg))
				gWatch.clear();
			else
			{
				const char* key = luaL_optstring(L, arg, "");
				if (key)
					gWatch.erase(key);
			}
			return 0;
		}

		int Debug_Table(lua_State* L)
		{
			(void)L;
			return 0;
		}

		int User_SetClipboard(lua_State* L)
		{
			const int arg = lua_istable(L, 1) ? 2 : 1;
			const char* text = luaL_optstring(L, arg, "");
			const char* note = luaL_optstring(L, arg + 1, nullptr);
			if (!text)
			{
				lua_pushboolean(L, 0);
				return 1;
			}
			bool ok = false;
			if (OpenClipboard(nullptr))
			{
				EmptyClipboard();
				const size_t len = std::strlen(text);
				HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, len + 1);
				if (h)
				{
					void* p = GlobalLock(h);
					if (p)
					{
						std::memcpy(p, text, len + 1);
						GlobalUnlock(h);
						SetClipboardData(CF_TEXT, h);
						ok = true;
					}
				}
				CloseClipboard();
			}
			if (ok && note && note[0] && G::API && G::API->Log)
				G::API->Log(LOGL_INFO, ADDON_NAME, note);
			lua_pushboolean(L, ok ? 1 : 0);
			return 1;
		}
	}

	void RegisterApi(lua_State* L)
	{
		RegisterTypes(L);
		RegisterMarker(L);
		RegisterTrail(L);
		RegisterWorld(L);
		RegisterWorldTrail(L);
		RegisterPack(L);
		RegisterMumble(L);
		RegisterEvent(L);
		RegisterMenu(L);
		RegisterStorage(L);
		RegisterInstance(L);

		lua_newtable(L);
		lua_pushcfunction(L, User_SetClipboard);
		lua_setfield(L, -2, "SetClipboard");
		lua_setglobal(L, "User");

		lua_newtable(L);
		lua_pushcfunction(L, Debug_Print); lua_setfield(L, -2, "Print");
		lua_pushcfunction(L, Debug_Info); lua_setfield(L, -2, "Info");
		lua_pushcfunction(L, Debug_Warn); lua_setfield(L, -2, "Warn");
		lua_pushcfunction(L, Debug_Error); lua_setfield(L, -2, "Error");
		lua_pushcfunction(L, Debug_Watch); lua_setfield(L, -2, "Watch");
		lua_pushcfunction(L, Debug_ClearWatch); lua_setfield(L, -2, "ClearWatch");
		lua_pushcfunction(L, Debug_ShowMessage); lua_setfield(L, -2, "ShowMessage");
		lua_pushcfunction(L, Debug_Table); lua_setfield(L, -2, "Table");
		lua_setglobal(L, "Debug");

		/* Hero pack VersionCheck compares PathingVersion to Blish Pathing module. */
		lua_pushstring(L, "1.10.5");
		lua_setglobal(L, "PathingVersion");

		lua_getglobal(L, "World");
		if (lua_istable(L, -1))
		{
			lua_getfield(L, -1, "Print");
			lua_setglobal(L, "World_Print");
		}
		lua_pop(L, 1);
	}
}
