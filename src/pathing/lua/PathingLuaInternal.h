#pragma once

#include "PathingTrails.h"

#include <mutex>
#include <string>
#include <vector>

struct lua_State;

namespace PathingLuaDetail
{
	void RegisterApi(lua_State* L);
	void RegisterTypes(lua_State* L);
	void RegisterMarker(lua_State* L);
	void RegisterWorld(lua_State* L);
	void RegisterPack(lua_State* L);
	void RegisterMumble(lua_State* L);
	void RegisterEvent(lua_State* L);
	void RegisterMenu(lua_State* L);
	void RegisterTrail(lua_State* L);
	void RegisterWorldTrail(lua_State* L);
	void RegisterStorage(lua_State* L);
	void RegisterInstance(lua_State* L);

	void PushMarker(lua_State* L, PathingTrails::Marker* m);
	PathingTrails::Marker* CheckMarker(lua_State* L, int idx);

	void PushTrail(lua_State* L, PathingTrails::Trail* t);
	PathingTrails::Trail* CheckTrail(lua_State* L, int idx);

	void PushCategory(lua_State* L, const char* ns);
	void PushMenuRef(lua_State* L, int id);
	void PushBehavior(lua_State* L, const PathingTrails::Marker* m);
	void RequestCdnTexture(int assetId, char* idOut, size_t idLen);

	void PushVector3(lua_State* L, float x, float y, float z);
	bool ReadVector3(lua_State* L, int idx, float& x, float& y, float& z);

	void LogLuaError(lua_State* L, const char* ctx);

	/* Script registry — store all .lua from packs; Pack:Require / pack.lua entry run them. */
	void StoreScriptSource(const std::string& name, const std::string& source);
	bool RequireScript(lua_State* L, const char* path);
	void ClearScriptSources();
	void RunPackEntryPoints(lua_State* L);

	extern std::vector<PathingTrails::Marker>* gTickMarkers;
	extern std::vector<PathingTrails::Trail>* gTickTrails;

	std::mutex& DynMutex();
	std::vector<PathingTrails::Marker>& DynMarkers();
	void ClearDynMarkers();
	PathingTrails::Marker* FindMarkerByGuid(const char* guid);

	void ClearOnTick();
	void ClearMenus(lua_State* L);
	void DrawMenus();
	void RunOnTick(lua_State* L, float elapsedMs);
	int  Event_OnTick(lua_State* L);

	std::string NewGuidBase64();
}
