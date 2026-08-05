#include "PathingIndex.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "PathingLua.h"
#include "PathingLuaLoad.h"
#include "PathingPacks.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>
#include "miniz/miniz.h"

namespace PathingDetail
{
void IndexPack(const std::wstring& packPath, std::vector<IndexedTrail>& out,
	std::vector<IndexedPoi>& poisOut, std::vector<PathingTrails::Category>& menuOut,
	std::unordered_map<std::string, MarkerStyle>& stylesOut,
	uint32_t epoch, bool* openedOut)
{
	if (openedOut)
		*openedOut = false;
	std::vector<uint8_t> file;
	if (!ReadFileW(packPath, file, kMaxZipBytes))
		return;

	mz_zip_archive zip{};
	if (!mz_zip_reader_init_mem(&zip, file.data(), file.size(), 0))
	{
		if (G::API && G::API->Log)
		{
			std::string leaf;
			const size_t slash = packPath.find_last_of(L"\\/");
			const std::wstring wleaf = (slash == std::wstring::npos)
				? packPath : packPath.substr(slash + 1);
			for (wchar_t c : wleaf)
				if (c < 128) leaf.push_back(static_cast<char>(c));
			char msg[256];
			std::snprintf(msg, sizeof(msg), "Pathing: failed to open zip %s", leaf.c_str());
			G::API->Log(LOGL_WARNING, ADDON_NAME, msg);
		}
		return;
	}
	if (openedOut)
		*openedOut = true;

	const size_t startIdx = out.size();
	const int n = static_cast<int>(mz_zip_reader_get_num_files(&zip));
	std::unordered_map<std::string, uint32_t> categoryMapIds;
	for (int i = 0; i < n; ++i)
	{
		if (gEpoch.load(std::memory_order_acquire) != epoch)
			break;
		mz_zip_archive_file_stat st{};
		if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory)
			continue;
		std::string name(st.m_filename);
		std::replace(name.begin(), name.end(), '\\', '/');
		const std::string low = ToLower(name);
		if (low.size() < 5 || low.compare(low.size() - 4, 4, ".xml") != 0)
			continue;
		if (st.m_uncomp_size == 0 || st.m_uncomp_size > 8u * 1024u * 1024u)
			continue;

		size_t sz = 0;
		void* mem = mz_zip_reader_extract_to_heap(&zip, i, &sz, 0);
		if (!mem || sz == 0)
			continue;
		std::string xml(static_cast<char*>(mem), sz);
		mz_free(mem);
		CollectCategoryMapIds(xml, categoryMapIds);
		IndexXml(packPath, xml, out);
		IndexPoisXml(packPath, xml, poisOut, categoryMapIds);

		/* Merge every MarkerCategory tree (tw_aaa + detail XMLs) so we get
		   the same fine-grained toggles as the official Tekkit overlay. */
		if (ToLower(xml).find("<markercategory") != std::string::npos)
		{
			std::vector<PathingTrails::Category> parsed;
			ParseMarkerMenuXml(xml, parsed, stylesOut);
			for (PathingTrails::Category& root : parsed)
				MergeCategoryTree(menuOut, std::move(root));
		}
	}

	/* Resolve .trl mapIds while the pack zip is still in memory (critical —
	   without this, map loads re-scan every trail in every pack). Uses the
	   central-directory lookup + 8-byte header read, so it stays cheap. */
	for (size_t i = startIdx; i < out.size(); ++i)
	{
		if (gEpoch.load(std::memory_order_acquire) != epoch)
			break;
		out[i].fileIndex = ZipLocate(zip, out[i].entryName);
		out[i].mapId = PeekTrlMapId(zip, out[i].fileIndex);
	}

	PathingLuaLoad::FromZip(zip);

	mz_zip_reader_end(&zip);
}

void MergeCategoryTree(std::vector<PathingTrails::Category>& dest, PathingTrails::Category&& src)
{
	PathingTrails::Category* found = nullptr;
	for (PathingTrails::Category& c : dest)
	{
		if (c.path == src.path)
		{
			found = &c;
			break;
		}
	}
	if (!found)
	{
		dest.push_back(std::move(src));
		return;
	}
	if (!src.label.empty())
		found->label = std::move(src.label);
	if (!src.tip.empty())
		found->tip = std::move(src.tip);
	if (!src.separator)
		found->separator = false;
	for (PathingTrails::Category& ch : src.children)
		MergeCategoryTree(found->children, std::move(ch));
	src.children.clear();
}

void AddTypeCounts(
	const std::string& rawType,
	std::unordered_map<std::string, int>& counts)
{
	const std::string type = ToLower(rawType);
	size_t pos = 0;
	while (pos < type.size())
	{
		const size_t dot = type.find('.', pos);
		const size_t end = (dot == std::string::npos) ? type.size() : dot;
		++counts[type.substr(0, end)];
		if (dot == std::string::npos)
			break;
		pos = dot + 1;
	}
}

void ApplyItemCounts(
	std::vector<PathingTrails::Category>& nodes,
	const std::unordered_map<std::string, int>& counts)
{
	for (PathingTrails::Category& c : nodes)
	{
		auto it = counts.find(ToLower(c.path));
		c.trails = (it == counts.end()) ? 0 : it->second;
		ApplyItemCounts(c.children, counts);
	}
}

void WorkerLoop(uint32_t epoch, uint32_t firstMap)
{
	try
	{
		/* Clear heavy state on the worker — never on the UI/render thread
		   (Reload packs used to wipe the ~Tekkit index under the frame lock
		   and freeze Wine/Steam). */
		{
			std::lock_guard<std::mutex> lock(gMutex);
			if (gEpoch.load(std::memory_order_acquire) != epoch)
				return;
			gIndex.clear();
			gPoiIndex.clear();
			gCategoryStyles.clear();
			gMenu.clear();
			gCurrentAll.clear();
			gCurrentMarkers.clear();
			gPackNames.clear();
			gGuide = {};
			gGuideActive = false;
			gMenuRevision.fetch_add(1, std::memory_order_release);
			gContentRevision.fetch_add(1, std::memory_order_release);
		}
		{
			std::lock_guard<std::mutex> lock(gIconMutex);
			gPendingIcons.clear();
			gIconQueued.clear();
			gIconRetain.clear();
		}
		gPackCount.store(0, std::memory_order_release);

		PathingLua::ClearScripts();

		/* Download / refresh curated packs into our pathing/ first (worker only).
		   User-dropped .taco files are never removed. */
		{
			const std::wstring ourPathing = AddonPaths::DataDir() + L"\\pathing";
			if (!ourPathing.empty())
				PathingPacks::EnsureCurated(ourPathing.c_str());
		}
		if (gEpoch.load(std::memory_order_acquire) != epoch)
			return;

		std::vector<std::wstring> dirs;
		DiscoverPackDirs(dirs);

		std::vector<std::wstring> ourDirs;
		std::vector<std::wstring> fallbackDirs;
		for (const std::wstring& d : dirs)
		{
			if (IsOurPathingDir(d))
				ourDirs.push_back(d);
			else
				fallbackDirs.push_back(d);
		}

		/* Prefer our pathing/ only — indexing Tekkit from both our folder and
		   Taimi doubled ~48MB zip + parsed data and could OOM/crash Wine. */
		std::vector<std::wstring> packs;
		for (const std::wstring& d : ourDirs)
			ListTacoFiles(d, packs, false);
		if (packs.empty())
		{
			for (const std::wstring& d : fallbackDirs)
				ListTacoFiles(d, packs, true); /* Tekkit seed only */
		}
		SuppressDuplicateTacoPacks(packs);

		/* Soft cap — curated + a few user packs; huge dumps still blow Wine. */
		constexpr size_t kMaxPacks = 12;
		if (packs.size() > kMaxPacks)
			packs.resize(kMaxPacks);

		std::vector<IndexedTrail> index;
		std::vector<IndexedPoi> pois;
		std::vector<PathingTrails::Category> menu;
		std::unordered_map<std::string, MarkerStyle> categoryStyles;
		index.reserve(4096);
		pois.reserve(16384);
		int packCount = 0;
		std::vector<std::string> packNames;
		for (const std::wstring& pack : packs)
		{
			if (gEpoch.load(std::memory_order_acquire) != epoch)
				return;
			const size_t before = index.size() + pois.size();
			const size_t menuBefore = menu.size();
			bool opened = false;
			IndexPack(pack, index, pois, menu, categoryStyles, epoch, &opened);
			/* List any pack we successfully opened as a zip — Hero/Blish packs
			   used to vanish from Overview when MapID inheritance left 0 POIs. */
			if (opened || index.size() + pois.size() > before || menu.size() > menuBefore)
			{
				++packCount;
				packNames.push_back(WideLeafUtf8(pack));
			}
		}

		/* Prepare the large menu entirely on the worker without holding the
		   render-thread mutex. Keep every MarkerCategory Tekkit ships —
		   do NOT prune POI-only / zero-trail nodes (official has those toggles). */
		std::unordered_map<std::string, int> itemCounts;
		itemCounts.reserve(index.size() + pois.size());
		for (const IndexedTrail& trail : index)
			AddTypeCounts(trail.type, itemCounts);
		for (const IndexedPoi& poi : pois)
			AddTypeCounts(poi.type, itemCounts);
		ApplyItemCounts(menu, itemCounts);

		{
			if (gEpoch.load(std::memory_order_acquire) != epoch)
				return;
			std::lock_guard<std::mutex> lock(gMutex);
			if (gEpoch.load(std::memory_order_acquire) != epoch)
				return;
			gIndex = std::move(index);
			gPoiIndex = std::move(pois);
			gCategoryStyles = std::move(categoryStyles);
			gMenu = std::move(menu);
			MarkEnabled(gMenu);
			gPackNames = std::move(packNames);
			gMenuRevision.fetch_add(1, std::memory_order_release);
			gPackCount.store(packCount, std::memory_order_release);
		}

		if (firstMap != 0 && gEpoch.load(std::memory_order_acquire) == epoch)
			LoadMapTrails(firstMap, epoch);
	}
	catch (...)
	{
	}
}

} // namespace PathingDetail
