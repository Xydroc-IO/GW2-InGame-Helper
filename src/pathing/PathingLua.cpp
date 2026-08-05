#include "PathingLua.h"
#include "PathingLuaInternal.h"

#include "Globals.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace
{
	lua_State* gL = nullptr;
	std::mutex gMu;
	std::unordered_set<std::string> gOnceDone;
	DWORD gLastTickMs = 0;

	/* Blish: script-once="Foo()" or "Foo(marker, 1)" — run with marker userdata. */
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
}

void PathingLua::Init()
{
	std::lock_guard<std::mutex> lock(gMu);
	if (gL)
		return;
	gL = luaL_newstate();
	if (!gL)
		return;
	luaL_openlibs(gL);
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
	if (!gL)
		return;
	PathingLuaDetail::ClearMenus(gL);
	lua_close(gL);
	gL = luaL_newstate();
	if (gL)
	{
		luaL_openlibs(gL);
		PathingLuaDetail::RegisterApi(gL);
	}
}

void PathingLua::AddScriptSource(const std::string& name, const std::string& source)
{
	Init();
	std::lock_guard<std::mutex> lock(gMu);
	if (!gL || source.empty())
		return;
	if (luaL_loadbuffer(gL, source.data(), source.size(), name.c_str()) != LUA_OK)
	{
		PathingLuaDetail::LogLuaError(gL, name.c_str());
		lua_pop(gL, 1);
		return;
	}
	if (lua_pcall(gL, 0, 0, 0) != LUA_OK)
	{
		PathingLuaDetail::LogLuaError(gL, name.c_str());
		lua_pop(gL, 1);
	}
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
