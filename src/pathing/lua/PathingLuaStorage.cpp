#include "PathingLuaInternal.h"

#include "AddonPaths.h"
#include "Globals.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

#include <windows.h>

namespace PathingLuaDetail
{
	namespace
	{
		std::mutex gStoreMu;
		std::unordered_map<std::string, std::string> gKv;

		std::wstring StorePath()
		{
			return AddonPaths::ConfigDir() + L"\\pathing_lua_storage.ini";
		}

		void LoadDisk()
		{
			std::lock_guard<std::mutex> lock(gStoreMu);
			if (!gKv.empty())
				return;
			const std::wstring w = StorePath();
			FILE* f = _wfopen(w.c_str(), L"rb");
			if (!f)
				return;
			char line[1024];
			while (std::fgets(line, sizeof(line), f))
			{
				char* nl = std::strchr(line, '\n');
				if (nl) *nl = 0;
				char* eq = std::strchr(line, '=');
				if (!eq || eq == line)
					continue;
				*eq = 0;
				gKv[line] = eq + 1;
			}
			std::fclose(f);
		}

		void SaveDisk()
		{
			const std::wstring w = StorePath();
			FILE* f = _wfopen(w.c_str(), L"wb");
			if (!f)
				return;
			std::lock_guard<std::mutex> lock(gStoreMu);
			for (const auto& kv : gKv)
				std::fprintf(f, "%s=%s\n", kv.first.c_str(), kv.second.c_str());
			std::fclose(f);
		}

		std::string MakeKey(const char* ns, const char* name)
		{
			const char* useNs = (ns && ns[0]) ? ns : "000";
			if (!name || !name[0])
				return {};
			std::string k = useNs;
			k += '\\';
			k += name;
			if (k.size() > 130)
				k.resize(130);
			return k;
		}

		/* Storage:UpsertValue(ns, name, value) or (name, value) */
		int Storage_UpsertValue(lua_State* L)
		{
			LoadDisk();
			int arg = lua_istable(L, 1) ? 2 : 1;
			const char* a = luaL_optstring(L, arg, "");
			const char* b = luaL_optstring(L, arg + 1, "");
			const char* c = luaL_optstring(L, arg + 2, nullptr);
			std::string key;
			std::string value;
			if (c)
			{
				key = MakeKey(a, b);
				value = c;
			}
			else
			{
				key = MakeKey(nullptr, a);
				value = b ? b : "";
			}
			if (key.empty())
			{
				lua_pushnil(L);
				return 1;
			}
			{
				std::lock_guard<std::mutex> lock(gStoreMu);
				gKv[key] = value;
			}
			SaveDisk();
			lua_pushstring(L, value.c_str());
			return 1;
		}

		int Storage_ReadValue(lua_State* L)
		{
			LoadDisk();
			int arg = lua_istable(L, 1) ? 2 : 1;
			const char* a = luaL_optstring(L, arg, "");
			const char* b = luaL_optstring(L, arg + 1, nullptr);
			const std::string key = b ? MakeKey(a, b) : MakeKey(nullptr, a);
			if (key.empty())
			{
				lua_pushnil(L);
				return 1;
			}
			std::lock_guard<std::mutex> lock(gStoreMu);
			auto it = gKv.find(key);
			if (it == gKv.end())
				lua_pushnil(L);
			else
				lua_pushstring(L, it->second.c_str());
			return 1;
		}

		int Storage_DeleteValue(lua_State* L)
		{
			LoadDisk();
			int arg = lua_istable(L, 1) ? 2 : 1;
			const char* a = luaL_optstring(L, arg, "");
			const char* b = luaL_optstring(L, arg + 1, nullptr);
			const std::string key = b ? MakeKey(a, b) : MakeKey(nullptr, a);
			if (!key.empty())
			{
				{
					std::lock_guard<std::mutex> lock(gStoreMu);
					gKv.erase(key);
				}
				SaveDisk();
			}
			return 0;
		}
	}

	void RegisterStorage(lua_State* L)
	{
		lua_newtable(L);
		lua_pushcfunction(L, Storage_UpsertValue);
		lua_setfield(L, -2, "UpsertValue");
		lua_pushcfunction(L, Storage_ReadValue);
		lua_setfield(L, -2, "ReadValue");
		lua_pushcfunction(L, Storage_DeleteValue);
		lua_setfield(L, -2, "DeleteValue");
		lua_setglobal(L, "Storage");
	}
}
