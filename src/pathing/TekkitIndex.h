#pragma once

#include "TekkitTrails.h"
#include "TekkitParse.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <windows.h>

/* Pack discovery / indexing / taco load orchestration + shared TekkitDetail state. */
namespace TekkitDetail
{
	using TekkitParse::MarkerStyle;
	using TekkitParse::IndexedTrail;
	using TekkitParse::IndexedPoi;
	using TekkitParse::kMaxZipBytes;
	using TekkitParse::kMaxTrailFile;
	using TekkitParse::kMaxPointsPerTrail;
	using TekkitParse::ToLower;
	using TekkitParse::LooksLikeMapCompletion;
	using TekkitParse::ReadFileW;
	using TekkitParse::ZipLocate;
	using TekkitParse::ZipExtractIndex;
	using TekkitParse::ZipExtractEntry;
	using TekkitParse::ZipReadEntry;
	using TekkitParse::PeekTrlMapId;
	using TekkitParse::IndexXml;
	using TekkitParse::IndexPoisXml;
	using TekkitParse::CollectCategoryMapIds;
	using TekkitParse::ParseTrl;
	using TekkitParse::ParseMarkerMenuXml;
	using TekkitParse::ResolveStyle;

	extern std::mutex gMutex;
	extern std::atomic<uint32_t> gEpoch;
	extern std::atomic<uint32_t> gLoadGen;
	extern std::atomic<bool> gLoading;
	extern std::atomic<bool> gForceReload;
	extern std::atomic<bool> gIndexStarted;
	extern std::atomic<uint32_t> gEnabledGen;
	extern uint32_t gLoadedEnabledGen;
	extern std::atomic<int> gPackCount;
	extern std::vector<std::string> gPackNames;
	extern std::thread gWorker;

	extern std::vector<IndexedTrail> gIndex;
	extern std::vector<IndexedPoi> gPoiIndex;
	extern std::unordered_map<std::string, MarkerStyle> gCategoryStyles;
	extern std::vector<TekkitTrails::Category> gMenu;
	extern std::atomic<uint64_t> gMenuRevision;
	extern std::atomic<uint64_t> gContentRevision;
	extern uint32_t gActiveMap;
	extern std::vector<TekkitTrails::Trail> gCurrentAll;
	extern std::vector<TekkitTrails::Marker> gCurrentMarkers;
	extern std::vector<std::string> gEnabledPaths;

	struct PendingIcon
	{
		std::string id;
		std::vector<uint8_t> bytes;
	};
	extern std::mutex gIconMutex;
	extern std::vector<PendingIcon> gPendingIcons;
	extern std::unordered_map<std::string, bool> gIconQueued;
	extern bool gGuideActive;
	extern TekkitTrails::Trail gGuide;

	void MergeCategoryTree(std::vector<TekkitTrails::Category>& dest, TekkitTrails::Category&& src);
	void AddTypeCounts(const std::string& rawType, std::unordered_map<std::string, int>& counts);
	void ApplyItemCounts(std::vector<TekkitTrails::Category>& nodes,
		const std::unordered_map<std::string, int>& counts);
	void IndexPack(const std::wstring& packPath, std::vector<IndexedTrail>& out,
		std::vector<IndexedPoi>& poisOut, std::vector<TekkitTrails::Category>& menuOut,
		std::unordered_map<std::string, MarkerStyle>& stylesOut,
		uint32_t epoch, bool* openedOut = nullptr);
	void DiscoverPackDirs(std::vector<std::wstring>& dirs);
	bool IsOurPathingDir(const std::wstring& dir);
	void ListTacoFiles(const std::wstring& dir, std::vector<std::wstring>& out, bool tekkitOnly);
	void WorkerLoop(uint32_t epoch, uint32_t firstMap);

	/* Defined in TekkitTrails.cpp */
	void MarkEnabled(std::vector<TekkitTrails::Category>& nodes);
	void LoadMapTrails(uint32_t mapId, uint32_t epoch);
}
