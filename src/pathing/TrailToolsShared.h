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
	bool OpenAuthoringFolder();
	void CopyClipboard(const char* text);
	bool ReadMumblePose(uint32_t& mapId, float& x, float& y, float& z);
	std::string MakeGuidBase64();
}
