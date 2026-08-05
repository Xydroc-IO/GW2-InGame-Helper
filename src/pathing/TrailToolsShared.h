#pragma once

#include "PathingTrails.h"

#include <cstdint>
#include <string>
#include <vector>

/* Draft pack state for Trail Tools authoring (in-memory + disk under pathing/authoring/). */
namespace TrailToolsDetail
{
	struct CategoryNode
	{
		std::string name;
		std::string displayName;
		std::string iconFile;
		std::string texture;
		float       fadeNear = -1.f;
		float       fadeFar = -1.f;
		float       trailScale = 1.f;
		float       iconSize = 1.f;
		float       alpha = 1.f;
		uint32_t    color = 0; /* 0 = omit; else AARRGGBB */
		std::string schedule; /* Blish UTC cron; empty = always */
		float       scheduleDuration = 0.f;
		std::vector<CategoryNode> children;
	};

	struct DraftPoi
	{
		uint32_t    mapId = 0;
		float       x = 0.f;
		float       y = 0.f;
		float       z = 0.f;
		std::string type;
		std::string guid;
		int         behavior = 0;
		bool        autoTrigger = false;
		float       triggerRange = 2.f;
		float       resetLength = 0.f;
		bool        invertBehavior = false;
		float       fadeNear = -1.f;
		float       fadeFar = -1.f;
		float       alpha = 1.f;
		float       iconSize = 1.f;
		float       heightOffset = 1.5f;
		float       mapDisplaySize = 20.f;
		bool        minimapVisible = true;
		bool        inGameVisible = true;
		std::string tipName;
		std::string tipDescription;
		std::string info;
		std::string copy;
		std::string copyMessage;
		std::string schedule;
		float       scheduleDuration = 0.f;
		std::string iconFile;
		std::string hide;
		std::string show;
		std::string scriptOnce;
		std::string scriptTrigger;
		std::string scriptFilter;
		std::string scriptTick;
		std::string scriptFocus;
	};

	struct DraftTrail
	{
		std::string fileRel; /* e.g. Data/Pack/Trails/Trail.trl */
		std::string type;    /* category path e.g. pack.t.extrail */
		uint32_t    mapId = 0;
		/* World meters X Y Z (Y up). (0,0,0) = section break. */
		std::vector<PathingTrails::WorldPoint> points;
	};

	struct DraftPack
	{
		char          packName[64] = "ExamplePack";
		char          displayName[96] = "Example Pack";
		CategoryNode  root;
		std::vector<DraftPoi>   pois;
		std::vector<DraftTrail> trails;
		DraftTrail    active; /* currently recording */
		char          markerType[160] = {};
		char          trailType[160] = {};
		char          trailFileStem[64] = "Trail";
		char          status[384] = {};
		bool          previewEnabled = true;
		int           selectedPoi = -1;
		int           selectedTrail = -1;
	};

	extern DraftPack gDraft;
	extern bool      gPlaceOnce;
	extern bool      gFocus;
	extern int       gTab;

	void SetStatus(const char* fmt, ...);
	void SeedDefaultCategories();
	void SanitizePackName(char* name, size_t len);
	std::string RootCategoryName();
	std::string CategoryPath(const CategoryNode& node, const std::string& parentPath);
	void CollectLeafPaths(const CategoryNode& node, const std::string& parentPath,
		std::vector<std::string>& out, bool trailLeaves);
	std::wstring AuthoringRoot();
	std::wstring PackDir();
	bool EnsureWorkspace();
	bool WriteDefaultAssets(); /* ExampleMarker.png + Trail.png if missing */
	bool OpenAuthoringFolder();
	void CopyClipboard(const char* text);
	bool ReadMumblePose(uint32_t& mapId, float& x, float& y, float& z);
	std::string MakeGuidBase64();
	bool HasDraftPreview(); /* trail pts or POIs on current map */
	CategoryNode* FindCategoryByPath(CategoryNode& node, const std::string& wantPath,
		const std::string& parentPath = {});
	void ApplyTrailLookPreset(int presetIndex);
	void ApplyMarkerLookPreset(int presetIndex);
	const char* const* TrailLookPresetNames(int* count);
	const char* const* MarkerLookPresetNames(int* count);

	/* Session + import (TrailToolsPersist / TrailToolsImport). */
	bool SaveDraftSession();
	bool LoadDraftSession();
	bool ImportTacoToDraft(const std::wstring& tacoPath, std::string& err);
}
