#include "TekkitIndex.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "PathingPacks.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>
#include <shlobj.h>
#include "miniz/miniz.h"

namespace TekkitDetail
{
std::string WideLeafUtf8(const std::wstring& path);

void IndexPack(const std::wstring& packPath, std::vector<IndexedTrail>& out,
	std::vector<IndexedPoi>& poisOut, std::vector<TekkitTrails::Category>& menuOut,
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
			std::vector<TekkitTrails::Category> parsed;
			ParseMarkerMenuXml(xml, parsed, stylesOut);
			for (TekkitTrails::Category& root : parsed)
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

	mz_zip_reader_end(&zip);
}

void DiscoverPackDirs(std::vector<std::wstring>& dirs)
{
	auto canonicalize = [](const std::wstring& d) -> std::wstring
	{
		wchar_t full[MAX_PATH]{};
		const DWORD n = GetFullPathNameW(d.c_str(), MAX_PATH, full, nullptr);
		if (n > 0 && n < MAX_PATH)
			return full;
		return d;
	};

	auto add = [&](const std::wstring& d)
	{
		const std::wstring canon = canonicalize(d);
		DWORD attr = GetFileAttributesW(canon.c_str());
		if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
			return;
		for (const std::wstring& e : dirs)
			if (_wcsicmp(e.c_str(), canon.c_str()) == 0)
				return;
		dirs.push_back(canon);
	};

	/* Our bundled pack lives here — no other addons required. */
	auto addOurs = [&](const std::wstring& addons)
	{
		add(addons + L"\\GW2-InGame-Helper\\pathing");
		add(addons + L"\\GW2-InGame-Helper\\pathing"); /* shipping pack if present */
		/* Reuse pack if already installed for Minimap Resizer. */
		add(addons + L"\\GW2-MinimapResizer\\pathing");
	};

	/* Optional fallbacks only if the user already has packs elsewhere. */
	auto addFallbacks = [&](const std::wstring& addons)
	{
		add(addons + L"\\Taimi\\pathing");
		add(addons + L"\\blishhud\\markers");
		add(addons + L"\\GW2TacO\\POIs");
	};

	auto addFromGameRoot = [&](const std::wstring& root)
	{
		addOurs(root + L"\\addons");
		addFallbacks(root + L"\\addons");
	};

	/* Prefer our DLL path (…/addons/GW2-InGame-Helper[/].dll) — reliable under Wine. */
	if (G::Self)
	{
		wchar_t img[MAX_PATH]{};
		const DWORD n = GetModuleFileNameW(G::Self, img, MAX_PATH);
		if (n > 0 && n < MAX_PATH)
		{
			std::wstring p(img);
			size_t slash = p.find_last_of(L"\\/");
			if (slash != std::wstring::npos)
				p = p.substr(0, slash); /* directory containing the DLL */
			slash = p.find_last_of(L"\\/");
			if (slash != std::wstring::npos)
			{
				const std::wstring leaf = p.substr(slash + 1);
				if (_wcsicmp(leaf.c_str(), L"GW2-InGame-Helper") == 0 ||
					_wcsicmp(leaf.c_str(), L"GW2-InGame-Helper") == 0)
				{
					add(p + L"\\pathing");
					addOurs(p.substr(0, slash));
					addFallbacks(p.substr(0, slash));
				}
				else
				{
					addOurs(p); /* DLL lived directly in addons/ */
					addFallbacks(p);
				}
			}
		}
	}

	if (G::API && G::API->Paths_GetAddonDirectory)
	{
		const char* ad = G::API->Paths_GetAddonDirectory(ADDON_NAME);
		if (ad && ad[0])
		{
			wchar_t wad[MAX_PATH]{};
			if (MultiByteToWideChar(CP_UTF8, 0, ad, -1, wad, MAX_PATH) > 0)
			{
				std::wstring p(wad);
				while (!p.empty() && (p.back() == L'\\' || p.back() == L'/'))
					p.pop_back();
				add(p + L"\\pathing");
				const size_t slash = p.find_last_of(L"\\/");
				if (slash != std::wstring::npos)
				{
					addOurs(p.substr(0, slash));
					addFallbacks(p.substr(0, slash));
				}
			}
		}
	}

	/* Gw2-64.exe directory → game root\addons\… */
	wchar_t exe[MAX_PATH]{};
	const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
	if (n > 0 && n < MAX_PATH)
	{
		std::wstring p(exe);
		const size_t slash = p.find_last_of(L"\\/");
		if (slash != std::wstring::npos)
			addFromGameRoot(p.substr(0, slash));
	}

	wchar_t docs[MAX_PATH]{};
	if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, docs)))
	{
		std::wstring d(docs);
		add(d + L"\\Guild Wars 2\\addons\\blishhud\\markers");
		add(d + L"\\Guild Wars 2\\addons\\GW2TacO\\POIs");
	}
}

bool IsOurPathingDir(const std::wstring& dir)
{
	std::wstring low;
	low.reserve(dir.size());
	for (wchar_t c : dir)
	{
		if (c >= L'A' && c <= L'Z')
			low.push_back(static_cast<wchar_t>(c - L'A' + L'a'));
		else if (c == L'/')
			low.push_back(L'\\');
		else
			low.push_back(c);
	}
	return low.find(L"gw2-ingame-helper\\pathing") != std::wstring::npos;
}

std::wstring LeafLower(const std::wstring& path)
{
	size_t slash = path.find_last_of(L"\\/");
	std::wstring leaf = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
	for (wchar_t& c : leaf)
	{
		if (c >= L'A' && c <= L'Z')
			c = static_cast<wchar_t>(c - L'A' + L'a');
		else if (c == L'/')
			c = L'\\';
	}
	return leaf;
}

std::string WideLeafUtf8(const std::wstring& path)
{
	size_t slash = path.find_last_of(L"\\/");
	const std::wstring leaf = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
	char buf[MAX_PATH]{};
	if (WideCharToMultiByte(CP_UTF8, 0, leaf.c_str(), -1, buf, MAX_PATH, nullptr, nullptr) > 0)
		return buf;
	std::string fallback;
	for (wchar_t c : leaf)
		if (c < 128)
			fallback.push_back(static_cast<char>(c));
	return fallback;
}

void ListTacoFiles(const std::wstring& dir, std::vector<std::wstring>& out, bool tekkitOnly)
{
	const std::wstring pattern = dir + L"\\*.taco";
	WIN32_FIND_DATAW fd{};
	HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE)
		return;
	do
	{
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		std::wstring name = fd.cFileName;
		/* Reject stamp files like Foo.taco.ver (Wine *.taco can match them). */
		{
			std::wstring low = name;
			for (wchar_t& c : low)
				if (c >= L'A' && c <= L'Z')
					c = static_cast<wchar_t>(c - L'A' + L'a');
			if (low.size() < 5 || low.compare(low.size() - 5, 5, L".taco") != 0)
				continue;
		}
		if (tekkitOnly)
		{
			std::wstring low = name;
			for (wchar_t& c : low)
				if (c >= L'A' && c <= L'Z')
					c = static_cast<wchar_t>(c - L'A' + L'a');
			if (low.find(L"tekkit") == std::wstring::npos)
				continue;
		}
		wchar_t fullBuf[MAX_PATH]{};
		const std::wstring joined = dir + L"\\" + fd.cFileName;
		const DWORD n = GetFullPathNameW(joined.c_str(), MAX_PATH, fullBuf, nullptr);
		const std::wstring full = (n > 0 && n < MAX_PATH) ? fullBuf : joined;
		const std::wstring leafKey = LeafLower(full);

		bool dup = false;
		for (const std::wstring& e : out)
		{
			if (_wcsicmp(e.c_str(), full.c_str()) == 0 || LeafLower(e) == leafKey)
			{
				dup = true;
				break;
			}
		}
		if (!dup)
			out.push_back(full);
	} while (FindNextFileW(h, &fd));
	FindClose(h);
}

/* Drop alias copies of curated packs (same content, different filename).
   e.g. "Tekkit's All-In-One.taco" next to curated tw_ALL_IN_ONE.taco → every
   route draws twice in-world. */
void SuppressDuplicateTacoPacks(std::vector<std::wstring>& packs)
{
	bool hasCuratedTekkit = false;
	for (const std::wstring& p : packs)
	{
		if (LeafLower(p) == L"tw_all_in_one.taco")
		{
			hasCuratedTekkit = true;
			break;
		}
	}

	auto isTekkitAioAlias = [](const std::wstring& leaf) -> bool
	{
		if (leaf == L"tw_all_in_one.taco")
			return false;
		const bool hasTekkit = leaf.find(L"tekkit") != std::wstring::npos;
		const bool hasAio =
			leaf.find(L"all_in_one") != std::wstring::npos ||
			leaf.find(L"all-in-one") != std::wstring::npos ||
			leaf.find(L"allinone") != std::wstring::npos;
		return hasTekkit && hasAio;
	};

	if (hasCuratedTekkit)
	{
		packs.erase(
			std::remove_if(packs.begin(), packs.end(),
				[&](const std::wstring& p) { return isTekkitAioAlias(LeafLower(p)); }),
			packs.end());
	}

	/* Exact byte-size clones of an already-kept pack (any author). */
	std::vector<std::pair<std::wstring, ULONGLONG>> kept;
	kept.reserve(packs.size());
	std::vector<std::wstring> filtered;
	filtered.reserve(packs.size());
	for (const std::wstring& p : packs)
	{
		WIN32_FILE_ATTRIBUTE_DATA fad{};
		ULONGLONG sz = 0;
		if (GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fad))
			sz = (static_cast<ULONGLONG>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
		bool clone = false;
		if (sz > 0)
		{
			for (const auto& k : kept)
			{
				if (k.second == sz)
				{
					clone = true;
					break;
				}
			}
		}
		if (clone)
			continue;
		kept.push_back({p, sz});
		filtered.push_back(p);
	}
	packs.swap(filtered);
}

void MergeCategoryTree(std::vector<TekkitTrails::Category>& dest, TekkitTrails::Category&& src)
{
	TekkitTrails::Category* found = nullptr;
	for (TekkitTrails::Category& c : dest)
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
	for (TekkitTrails::Category& ch : src.children)
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
	std::vector<TekkitTrails::Category>& nodes,
	const std::unordered_map<std::string, int>& counts)
{
	for (TekkitTrails::Category& c : nodes)
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
		std::vector<TekkitTrails::Category> menu;
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

} // namespace TekkitDetail
