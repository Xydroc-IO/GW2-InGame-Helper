#pragma once

#include "PathingTrails.h"
#include "PathingParse.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>
#include <winhttp.h>

/* Pack discovery / indexing / taco load orchestration + shared PathingDetail state. */
namespace PathingDetail
{
	using PathingParse::MarkerStyle;
	using PathingParse::IndexedTrail;
	using PathingParse::IndexedPoi;
	using PathingParse::kMaxZipBytes;
	using PathingParse::kMaxTrailFile;
	using PathingParse::kMaxPointsPerTrail;
	using PathingParse::ToLower;
	using PathingParse::LooksLikeMapCompletion;
	using PathingParse::ReadFileW;
	using PathingParse::ZipLocate;
	using PathingParse::ZipExtractIndex;
	using PathingParse::ZipExtractEntry;
	using PathingParse::ZipReadEntry;
	using PathingParse::PeekTrlMapId;
	using PathingParse::IndexXml;
	using PathingParse::IndexPoisXml;
	using PathingParse::CollectCategoryMapIds;
	using PathingParse::ParseTrl;
	using PathingParse::ParseMarkerMenuXml;
	using PathingParse::ResolveStyle;
	using PathingParse::DecodeXmlEntities;

	constexpr size_t kMaxTrailsPerMap = 480;
	/* World XYZ mirrors ParseTrl (full-length sample, not start-only). */
	constexpr size_t kMaxMarkersPerMap = 800;
	constexpr size_t kMaxMinimapMarkers = 250;

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

	extern std::atomic<HINTERNET> gLiveSession;
	extern std::atomic<HINTERNET> gLiveRequest;

	extern std::vector<IndexedTrail> gIndex;
	extern std::vector<IndexedPoi> gPoiIndex;
	extern std::unordered_map<std::string, MarkerStyle> gCategoryStyles;
	extern std::vector<PathingTrails::Category> gMenu;
	extern std::atomic<uint64_t> gMenuRevision;
	extern std::atomic<uint64_t> gContentRevision;
	extern uint32_t gActiveMap;
	extern std::vector<PathingTrails::Trail> gCurrentAll;
	extern std::vector<PathingTrails::Marker> gCurrentMarkers;
	extern std::unordered_set<uint32_t> gMapsWithLadyBarefoot;
	extern std::vector<std::string> gEnabledPaths;

	struct PendingIcon
	{
		std::string id;
		std::vector<uint8_t> bytes;
	};
	extern std::mutex gIconMutex;
	extern std::vector<PendingIcon> gPendingIcons;
	extern std::unordered_map<std::string, bool> gIconQueued;
	/* Nexus may decode GetOrCreateFromMemory asynchronously (Wine). Keep PNG
	   bytes alive until Textures_Get()->Resource is non-null. */
	extern std::unordered_map<std::string, std::vector<uint8_t>> gIconRetain;
	extern bool gGuideActive;
	extern float gGuideDestX;
	extern float gGuideDestY;
	extern PathingTrails::Trail gGuide;
	extern float gGuidePlayerX;
	extern float gGuidePlayerY;
	extern bool gGuideHavePlayer;

	struct Rects
	{
		float mx0 = 0, my0 = 0, mx1 = 1, my1 = 1;
		float cx0 = 0, cy0 = 0, cx1 = 1, cy1 = 1;
		bool valid = false;
	};
	extern std::unordered_map<uint32_t, bool> gMapRectsReady;
	extern std::unordered_map<uint32_t, Rects> gRects;

	/* Shared zip reader for LoadMapTrails + QueueMapIcons (one pack in memory). */
	struct OpenPack
	{
		std::vector<uint8_t> file;
		mz_zip_archive zip{};
		bool ok = false;

		OpenPack() = default;
		OpenPack(const OpenPack&) = delete;
		OpenPack& operator=(const OpenPack&) = delete;

		~OpenPack() { Close(); }

		void Close()
		{
			if (ok)
			{
				mz_zip_reader_end(&zip);
				ok = false;
			}
			file.clear();
		}

		bool Open(const std::wstring& path)
		{
			Close();
			if (!ReadFileW(path, file, kMaxZipBytes))
				return false;
			std::memset(&zip, 0, sizeof(zip));
			if (!mz_zip_reader_init_mem(&zip, file.data(), file.size(), 0))
			{
				file.clear();
				return false;
			}
			ok = true;
			return true;
		}
	};

	void MergeCategoryTree(std::vector<PathingTrails::Category>& dest, PathingTrails::Category&& src);
	void AddTypeCounts(const std::string& rawType, std::unordered_map<std::string, int>& counts);
	void ApplyItemCounts(std::vector<PathingTrails::Category>& nodes,
		const std::unordered_map<std::string, int>& counts);
	void IndexPack(const std::wstring& packPath, std::vector<IndexedTrail>& out,
		std::vector<IndexedPoi>& poisOut, std::vector<PathingTrails::Category>& menuOut,
		std::unordered_map<std::string, MarkerStyle>& stylesOut,
		uint32_t epoch, bool* openedOut = nullptr);
	void DiscoverPackDirs(std::vector<std::wstring>& dirs);
	bool IsOurPathingDir(const std::wstring& dir);
	std::wstring LeafLower(const std::wstring& path);
	std::string WideLeafUtf8(const std::wstring& path);
	void ListTacoFiles(const std::wstring& dir, std::vector<std::wstring>& out, bool tekkitOnly);
	void SuppressDuplicateTacoPacks(std::vector<std::wstring>& packs);
	void WorkerLoop(uint32_t epoch, uint32_t firstMap);

	/* PathingLoad*.cpp */
	void AbortHttp();
	bool FetchMapRects(uint32_t mapId, Rects& r);
	void WorldToContinent(const Rects& r, float wxMeters, float wzMeters, float& cx, float& cy);
	bool PrefixMatchesType(const std::string& typeLow, const std::string& prefixLow);
	bool LadyMapRouteEdition(const std::string& typeLow, std::string& outEdition);
	bool IsLadyWithMountsEdition(const std::string& seg);
	bool TypeHasLadyMountShortcut(const std::string& typeLow);
	bool TypeEnabledLocked(const std::string& type);
	bool TypeCategoryEnabled(const std::string& type, const std::vector<std::string>& enabled);
	bool TypeEnabledWithEnabled(const std::string& type, const std::vector<std::string>& enabled);
	bool MarkerShownInWorld(const PathingTrails::Marker& marker);
	bool MarkerShownOnCompass(const PathingTrails::Marker& marker);
	bool IsMountShortcutMarker(const PathingTrails::Marker& marker);
	void RebuildSearchGuideLocked();
	bool CategoryUiEnabledLocked(const std::string& path);
	void InsertCatPath(std::vector<PathingTrails::Category>& roots, const std::string& type);
	void MarkEnabled(std::vector<PathingTrails::Category>& nodes);
	void EnsureRootEnabledLocked(const char* root); /* gMutex held */
	bool IsTekkitMcPath(const std::string& p);
	void RestoreNonMcTekkitSiblingsLocked(bool hadBroadTwGuides); /* gMutex held */
	std::string IconTextureId(const std::string& iconFile);
	void QueueMapIcons(std::unordered_map<std::string, std::wstring>& assetsNeeded,
		const std::wstring& preferredPack, uint32_t epoch);
	void LoadMapTrails(uint32_t mapId, uint32_t epoch);

	/* PathingTrailsCore.cpp */
	bool MarkerBehaviorVisible(const PathingTrails::Marker& m);
}
