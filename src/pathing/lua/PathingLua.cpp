#include "PathingLua.h"
#include "PathingLuaInternal.h"

#include "Globals.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <cctype>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace PathingLuaDetail
{
	namespace
	{
		std::unordered_map<std::string, std::string> gScriptSources;
		std::unordered_set<std::string> gScriptRequired;
		std::vector<std::string> gPendingPackEntries;

		std::string NormalizeScriptKey(const char* path)
		{
			if (!path)
				return {};
			std::string s = path;
			for (char& c : s)
			{
				if (c == '\\')
					c = '/';
				else
					c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			while (!s.empty() && s[0] == '/')
				s.erase(s.begin());
			if (s.size() < 4 || s.compare(s.size() - 4, 4, ".lua") != 0)
				s += ".lua";
			return s;
		}
	}

	void StoreScriptSource(const std::string& name, const std::string& source)
	{
		if (source.empty())
			return;
		const std::string key = NormalizeScriptKey(name.c_str());
		if (key.empty())
			return;
		gScriptSources[key] = source;
		/* Track pack.lua entry points (any path ending in /pack.lua or pack.lua). */
		if (key == "pack.lua" ||
			(key.size() >= 9 && key.compare(key.size() - 9, 9, "/pack.lua") == 0))
			gPendingPackEntries.push_back(key);
	}

	bool RequireScript(lua_State* L, const char* path)
	{
		if (!L)
			return false;
		const std::string key = NormalizeScriptKey(path);
		if (key.empty())
			return false;
		if (gScriptRequired.count(key))
			return true;
		auto it = gScriptSources.find(key);
		if (it == gScriptSources.end())
		{
			/* Try basename match for Require("Storage") -> scripts/utility/storage.lua */
			const std::string base = key;
			for (const auto& kv : gScriptSources)
			{
				if (kv.first.size() >= base.size() &&
					kv.first.compare(kv.first.size() - base.size(), base.size(), base) == 0 &&
					(kv.first.size() == base.size() ||
						kv.first[kv.first.size() - base.size() - 1] == '/'))
				{
					it = gScriptSources.find(kv.first);
					break;
				}
			}
		}
		if (it == gScriptSources.end())
			return false;
		gScriptRequired.insert(it->first);
		gScriptRequired.insert(key);
		if (luaL_loadbuffer(L, it->second.data(), it->second.size(), it->first.c_str()) != LUA_OK)
		{
			LogLuaError(L, it->first.c_str());
			lua_pop(L, 1);
			return false;
		}
		if (lua_pcall(L, 0, 0, 0) != LUA_OK)
		{
			LogLuaError(L, it->first.c_str());
			lua_pop(L, 1);
			return false;
		}
		return true;
	}

	void ClearScriptSources()
	{
		gScriptSources.clear();
		gScriptRequired.clear();
		gPendingPackEntries.clear();
	}

	void RunPackEntryPoints(lua_State* L)
	{
		if (!L)
			return;
		std::vector<std::string> entries = gPendingPackEntries;
		gPendingPackEntries.clear();
		for (const std::string& e : entries)
			RequireScript(L, e.c_str());
	}
}

namespace
{
	lua_State* gL = nullptr;
	std::mutex gMu;
	std::unordered_set<std::string> gOnceDone;
	DWORD gLastTickMs = 0;

	/* Blish: script-once="Foo()" or "Foo(marker, 1)" - run with marker userdata. */
	bool CallScriptAttr(lua_State* L, const char* expr, PathingTrails::Marker* m, bool* outBool)
	{
		if (!L || !expr || !expr[0] || !m)
			return false;

		PathingLuaDetail::PushMarker(L, m);
		lua_setglobal(L, "marker");

		std::string wrapped = "local marker = marker; return (function() return ";
		wrapped += expr;
		wrapped += "; end)()";

		if (luaL_loadstring(L, wrapped.c_str()) != LUA_OK)
		{
			PathingLuaDetail::LogLuaError(L, "load");
			lua_pop(L, 1);
			if (luaL_loadstring(L, expr) != LUA_OK)
			{
				PathingLuaDetail::LogLuaError(L, "load-raw");
				lua_pop(L, 1);
				return false;
			}
		}
		if (lua_pcall(L, 0, 1, 0) != LUA_OK)
		{
			PathingLuaDetail::LogLuaError(L, expr);
			lua_pop(L, 1);
			return false;
		}
		if (outBool)
		{
			if (lua_isboolean(L, -1))
				*outBool = lua_toboolean(L, -1) != 0;
			else if (lua_isnil(L, -1))
				*outBool = true;
			else
				*outBool = true;
		}
		lua_settop(L, 0);
		return true;
	}

	void NilField(lua_State* L, const char* name)
	{
		lua_pushnil(L);
		lua_setfield(L, -2, name);
	}

	/* Blish script-* needs base/table/string/math. Do not open io / package / debug.
	   Keep os.time / os.clock / os.date; strip execute, filesystem, getenv. */
	void OpenSafeLibs(lua_State* L)
	{
		static const luaL_Reg kLibs[] = {
			{LUA_GNAME, luaopen_base},
			{LUA_COLIBNAME, luaopen_coroutine},
			{LUA_TABLIBNAME, luaopen_table},
			{LUA_STRLIBNAME, luaopen_string},
			{LUA_MATHLIBNAME, luaopen_math},
			{LUA_UTF8LIBNAME, luaopen_utf8},
			{LUA_OSLIBNAME, luaopen_os},
			{nullptr, nullptr}
		};
		for (const luaL_Reg* lib = kLibs; lib->func; ++lib)
		{
			luaL_requiref(L, lib->name, lib->func, 1);
			lua_pop(L, 1);
		}

		lua_pushnil(L);
		lua_setglobal(L, "loadfile");
		lua_pushnil(L);
		lua_setglobal(L, "dofile");

		lua_getglobal(L, "os");
		if (lua_istable(L, -1))
		{
			NilField(L, "execute");
			NilField(L, "remove");
			NilField(L, "rename");
			NilField(L, "exit");
			NilField(L, "setlocale");
			NilField(L, "tmpname");
			NilField(L, "getenv");
		}
		lua_pop(L, 1);
	}
}

void PathingLua::Init()
{
	std::lock_guard<std::mutex> lock(gMu);
	if (gL)
		return;
	gL = luaL_newstate();
	if (!gL)
		return;
	OpenSafeLibs(gL);
	PathingLuaDetail::RegisterApi(gL);
}

void PathingLua::Shutdown()
{
	std::lock_guard<std::mutex> lock(gMu);
	if (gL)
	{
		PathingLuaDetail::ClearOnTick();
		PathingLuaDetail::ClearMenus(gL);
		lua_close(gL);
		gL = nullptr;
	}
	gOnceDone.clear();
	PathingLuaDetail::gTickMarkers = nullptr;
	PathingLuaDetail::gTickTrails = nullptr;
	PathingLuaDetail::ClearDynMarkers();
}

bool PathingLua::Enabled()
{
	return G::EnablePathingLua;
}

void PathingLua::SetEnabled(bool on)
{
	G::EnablePathingLua = on;
	if (on)
		Init();
}

void PathingLua::ClearScripts()
{
	std::lock_guard<std::mutex> lock(gMu);
	gOnceDone.clear();
	PathingLuaDetail::ClearOnTick();
	PathingLuaDetail::ClearDynMarkers();
	PathingLuaDetail::ClearScriptSources();
	if (!gL)
		return;
	PathingLuaDetail::ClearMenus(gL);
	lua_close(gL);
	gL = luaL_newstate();
	if (gL)
	{
		OpenSafeLibs(gL);
		PathingLuaDetail::RegisterApi(gL);
	}
}

void PathingLua::AddScriptSource(const std::string& name, const std::string& source)
{
	Init();
	std::lock_guard<std::mutex> lock(gMu);
	PathingLuaDetail::StoreScriptSource(name, source);
}

void PathingLua::RunPendingPackEntries()
{
	if (!Enabled())
		return;
	Init();
	std::lock_guard<std::mutex> lock(gMu);
	if (gL)
		PathingLuaDetail::RunPackEntryPoints(gL);
}

void PathingLua::OnMarkersLoaded(std::vector<PathingTrails::Marker>& markers)
{
	if (!Enabled())
		return;
	Init();
	std::lock_guard<std::mutex> lock(gMu);
	if (!gL)
		return;
	PathingLuaDetail::gTickMarkers = &markers;
	for (auto& m : markers)
	{
		if (m.scriptOnce.empty() || !m.guid[0])
			continue;
		if (gOnceDone.count(m.guid))
			continue;
		CallScriptAttr(gL, m.scriptOnce.c_str(), &m, nullptr);
		gOnceDone.insert(m.guid);
	}
	PathingLuaDetail::gTickMarkers = nullptr;
}

void PathingLua::Tick(std::vector<PathingTrails::Marker>& markers)
{
	if (!Enabled())
		return;
	Init();
	std::lock_guard<std::mutex> lock(gMu);
	if (!gL)
		return;

	PathingLuaDetail::gTickMarkers = &markers;
	const DWORD now = GetTickCount();
	const bool doTick = (now - gLastTickMs) >= 50;
	const float elapsed = doTick ? static_cast<float>(now - gLastTickMs) : 0.f;
	if (doTick)
		gLastTickMs = now;

	float ax = 0.f, ay = 0.f, az = 0.f;
	if (G::Mumble && G::Mumble->uiTick)
	{
		ax = G::Mumble->fAvatarPosition[0];
		ay = G::Mumble->fAvatarPosition[1];
		az = G::Mumble->fAvatarPosition[2];
	}

	auto process = [&](PathingTrails::Marker& m) {
		if (m.luaRemoved)
		{
			m.luaHidden = true;
			return;
		}
		if (!m.scriptFilter.empty())
		{
			bool ok = true;
			CallScriptAttr(gL, m.scriptFilter.c_str(), &m, &ok);
			m.luaHidden = !ok;
		}
		if (m.luaHidden)
			return;

		if (!m.scriptTrigger.empty() && m.autoTrigger && m.triggerRange > 0.f)
		{
			const float dx = m.world.x - ax;
			const float dy = m.world.y - ay;
			const float dz = m.world.z - az;
			if (dx * dx + dy * dy + dz * dz <= m.triggerRange * m.triggerRange)
				CallScriptAttr(gL, m.scriptTrigger.c_str(), &m, nullptr);
		}
		if (doTick && !m.scriptTick.empty())
			CallScriptAttr(gL, m.scriptTick.c_str(), &m, nullptr);
		if (!m.scriptFocus.empty() && m.triggerRange > 0.f)
		{
			const float dx = m.world.x - ax;
			const float dy = m.world.y - ay;
			const float dz = m.world.z - az;
			if (dx * dx + dy * dy + dz * dz <= m.triggerRange * m.triggerRange)
				CallScriptAttr(gL, m.scriptFocus.c_str(), &m, nullptr);
		}
	};

	for (auto& m : markers)
		process(m);

	{
		std::lock_guard<std::mutex> dlock(PathingLuaDetail::DynMutex());
		for (auto& m : PathingLuaDetail::DynMarkers())
			process(m);
	}

	if (doTick)
		PathingLuaDetail::RunOnTick(gL, elapsed);

	PathingLuaDetail::gTickMarkers = nullptr;
}

void PathingLua::AppendDynamicMarkers(uint32_t mapId, std::vector<PathingTrails::Marker>& out)
{
	if (!Enabled())
		return;
	std::lock_guard<std::mutex> lock(PathingLuaDetail::DynMutex());
	for (const auto& m : PathingLuaDetail::DynMarkers())
	{
		if (m.luaRemoved || m.luaHidden)
			continue;
		if (mapId != 0 && m.mapId != 0 && m.mapId != mapId)
			continue;
		out.push_back(m);
	}
}
