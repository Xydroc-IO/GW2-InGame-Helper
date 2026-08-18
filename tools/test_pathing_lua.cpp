/* Wine smoke: Blish-shaped PathingLua API (SetPos + CreateMarker + gaps). */
#include "PathingLua.h"
#include "PathingLuaInternal.h"

#include "Globals.h"
#include "PathingIndex.h"
#include "PathingTrails.h"
#include "MarkerBehaviors.h"
#include "MumbleIdentity.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace G
{
	AddonAPI_t* API = nullptr;
	NexusLinkData_t* NexusLink = nullptr;
	MumbleLinkedMem* Mumble = nullptr;
	bool EnablePathingLua = true;
}

namespace PathingTrails
{
	void SetCategoryEnabled(const std::string&, bool) {}
	std::vector<std::string> EnabledPaths() { return {}; }
}

namespace MarkerBehaviors
{
	void RequestInteract() {}
}

namespace MumbleIdentity
{
	const char* CharacterName() { return "Test"; }
}

/* PathingLuaMenu.cpp (ImGui) is not linked here. */
namespace PathingLuaDetail
{
	static int Menu_AddStub(lua_State* L)
	{
		int arg = 1;
		if (lua_istable(L, 1) && lua_type(L, 2) == LUA_TSTRING)
			arg = 2;
		const char* name = luaL_checkstring(L, arg);
		luaL_checktype(L, arg + 1, LUA_TFUNCTION);
		lua_newtable(L);
		lua_pushstring(L, name);
		lua_setfield(L, -2, "Name");
		lua_pushinteger(L, 1);
		lua_setfield(L, -2, "Id");
		return 1;
	}
	static int Menu_RemoveStub(lua_State*) { return 0; }

	void RegisterMenu(lua_State* L)
	{
		lua_newtable(L);
		lua_pushcfunction(L, Menu_AddStub);
		lua_setfield(L, -2, "Add");
		lua_pushcfunction(L, Menu_RemoveStub);
		lua_setfield(L, -2, "Remove");
		lua_setglobal(L, "Menu");
	}
	void ClearMenus(lua_State*) {}
	void DrawMenus() {}
	void RegisterStorage(lua_State* L)
	{
		lua_newtable(L);
		lua_setglobal(L, "Storage");
	}
	void RegisterInstance(lua_State* L)
	{
		lua_newtable(L);
		lua_setglobal(L, "I");
	}
}

namespace PathingDetail
{
	std::mutex gMutex;
	std::vector<PathingTrails::Trail> gCurrentAll;
	std::mutex gIconMutex;
	std::vector<PendingIcon> gPendingIcons;
	std::unordered_map<std::string, bool> gIconQueued;
	std::unordered_map<std::string, std::vector<uint8_t>> gIconRetain;
}

void PathingLua::DrawScriptMenus() {}

static int Fail(const char* m)
{
	std::fprintf(stderr, "FAIL: %s\n", m);
	return 1;
}

int main()
{
	PathingLua::SetEnabled(true);
	PathingLua::Init();

	PathingLua::AddScriptSource("follow.lua", R"LUA(
assert(io == nil)
assert(package == nil)
assert(debug == nil)
assert(loadfile == nil)
assert(dofile == nil)
assert(os.execute == nil)
assert(os.getenv == nil)
assert(type(os.time) == "function")

function FollowPlayer(marker)
  local x, y, z = Mumble.PlayerPosition()
  marker:SetPos(x + 1, y, z)
  marker.Tint = 0xFF00FF00
end

function AlwaysShow(marker)
  return true
end
)LUA");
	PathingLua::AddScriptSource("pack.lua", "Pack:Require('follow.lua')");
	PathingLua::RunPendingPackEntries();

	std::vector<PathingTrails::Marker> markers(1);
	auto& m = markers[0];
	m.mapId = 50;
	m.world = { 10.f, 20.f, 30.f };
	m.behavior = 4;
	std::snprintf(m.guid, sizeof(m.guid), "testguid==");
	m.scriptTick = "FollowPlayer(marker)";
	m.scriptFilter = "AlwaysShow(marker)";
	m.scriptOnce = "FollowPlayer(marker)";

	static MumbleLinkedMem mem{};
	mem.uiTick = 1;
	mem.fAvatarPosition[0] = 100.f;
	mem.fAvatarPosition[1] = 50.f;
	mem.fAvatarPosition[2] = 200.f;
	static unsigned char ctxBuf[256]{};
	auto* ctx = reinterpret_cast<MumbleContext*>(ctxBuf);
	ctx->mapId = 50;
	std::memcpy(mem.context, ctxBuf, sizeof(mem.context));
	G::Mumble = &mem;

	PathingLua::OnMarkersLoaded(markers);
	if (std::fabs(m.world.x - 101.f) > 0.01f)
		return Fail("script-once SetPos did not move marker");

	m.world = { 10.f, 20.f, 30.f };
	PathingLua::Tick(markers);
	if (std::fabs(m.world.x - 101.f) > 0.01f)
		return Fail("script-tick SetPos failed");
	if (m.color != 0xFF00FF00u)
		return Fail("Tint write failed");

	PathingLua::AddScriptSource("spawn/pack.lua", R"LUA(
local m = Pack:CreateMarker({ type = "demo.spawn", xpos = 1, ypos = 2, zpos = 3 })
m:SetPos(9, 8, 7)
)LUA");
	PathingLua::RunPendingPackEntries();

	std::vector<PathingTrails::Marker> dyn;
	PathingLua::AppendDynamicMarkers(50, dyn);
	if (dyn.empty())
		return Fail("CreateMarker produced no dynamic marker");
	if (std::fabs(dyn[0].world.x - 9.f) > 0.01f)
		return Fail("dynamic SetPos failed");

	static std::vector<PathingTrails::Trail> trails(1);
	trails[0].mapId = 50;
	std::snprintf(trails[0].label, sizeof(trails[0].label), "demo.trail");
	std::snprintf(trails[0].guid, sizeof(trails[0].guid), "trailguid==");
	trails[0].worldPoints = { { 0, 0, 0 }, { 10, 0, 10 } };
	trails[0].alpha = 1.f;
	PathingLuaDetail::gTickMarkers = &markers;
	PathingLuaDetail::gTickTrails = &trails;

	PathingLua::AddScriptSource("gaps/pack.lua", R"LUA(
local b = World:MarkerByGuid("testguid==")
assert(b ~= nil)
local beh = b:GetBehavior()
assert(beh ~= nil and beh.BehaviorType == 4)
assert(beh.Name == "ReappearOnWeeklyReset")

local item = Menu.Add("Smoke", function(m) end)
assert(item ~= nil and item.Name == "Smoke")

local t = World:TrailByGuid("trailguid==")
assert(t ~= nil)
t.Alpha = 0.5
t.TrailScale = 2
t:SetTexture(102491)
assert(t.Alpha == 0.5)
assert(t.TrailScale == 2)

t:Remove()
b:SetTexture(102491)
)LUA");
	PathingLua::RunPendingPackEntries();

	if (!trails[0].luaRemoved)
		return Fail("Trail:Remove did not set luaRemoved");
	if (std::fabs(trails[0].alpha - 0.5f) > 0.01f)
		return Fail("Trail Alpha write failed");
	if (std::strncmp(trails[0].textureId, "GW2IGH_CDN_102491", 17) != 0)
		return Fail("Trail CDN SetTexture id failed");
	if (std::strncmp(markers[0].iconId, "GW2IGH_CDN_102491", 17) != 0)
		return Fail("Marker CDN SetTexture id failed");

	PathingLuaDetail::gTickMarkers = nullptr;
	PathingLuaDetail::gTickTrails = nullptr;
	PathingLua::Shutdown();
	std::printf("OK pathing lua blish-shaped API\n");
	return 0;
}
