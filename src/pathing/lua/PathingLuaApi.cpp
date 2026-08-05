#include "PathingLuaInternal.h"

#include "Globals.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <cstring>
#include <windows.h>

namespace PathingLuaDetail
{
	namespace
	{
		int User_SetClipboard(lua_State* L)
		{
			const int arg = lua_istable(L, 1) ? 2 : 1;
			const char* text = luaL_optstring(L, arg, "");
			if (!text)
				return 0;
			if (!OpenClipboard(nullptr))
				return 0;
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
				}
			}
			CloseClipboard();
			return 0;
		}

		int Debug_Table(lua_State* L)
		{
			(void)L;
			return 0;
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

		lua_newtable(L);
		lua_pushcfunction(L, User_SetClipboard);
		lua_setfield(L, -2, "SetClipboard");
		lua_setglobal(L, "User");

		lua_newtable(L);
		lua_pushcfunction(L, Debug_Table);
		lua_setfield(L, -2, "Table");
		lua_setglobal(L, "Debug");

		/* Back-compat thin aliases from v1. */
		lua_getglobal(L, "World");
		if (lua_istable(L, -1))
		{
			lua_getfield(L, -1, "Print");
			lua_setglobal(L, "World_Print");
		}
		lua_pop(L, 1);
	}
}
