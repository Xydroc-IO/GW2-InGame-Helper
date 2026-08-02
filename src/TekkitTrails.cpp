#include "TekkitTrails.h"

#include "TekkitParse.h"

#include "Globals.h"
#include "Settings.h"

#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include "miniz/miniz.h"
#include <shlobj.h>

#include "imgui/imgui.h"

namespace
{
	using TekkitParse::MarkerStyle;
	using TekkitParse::IndexedTrail;
	using TekkitParse::IndexedPoi;
	using TekkitParse::kMaxZipBytes;
	using TekkitParse::kMaxTrailFile;
	using TekkitParse::kMaxPointsPerTrail;
	using TekkitParse::ToLower;
	using TekkitParse::LooksLikeMapCompletion;
	using TekkitParse::ParseColorAttr;
	using TekkitParse::Attr;
	using TekkitParse::ParseBoolValue;
	using TekkitParse::MergeStyle;
	using TekkitParse::ParseStyle;
	using TekkitParse::ResolveStyle;
	using TekkitParse::ReadFileW;
	using TekkitParse::ZipLocate;
	using TekkitParse::ZipExtractIndex;
	using TekkitParse::ZipExtractEntry;
	using TekkitParse::ZipReadEntry;
	using TekkitParse::PeekTrlMapId;
	using TekkitParse::IndexXml;
	using TekkitParse::IndexPoisXml;
	using TekkitParse::ParseTrl;
	using TekkitParse::ParseMarkerMenuXml;

	constexpr size_t kMaxTrailsPerMap = 180;
	/* World XYZ mirrors ParseTrl (full-length sample, not start-only). */
	constexpr size_t kMaxMarkersPerMap = 800;
	constexpr size_t kMaxMinimapMarkers = 250;

	std::mutex gMutex;
	std::atomic<uint32_t> gEpoch{0};
	std::atomic<uint32_t> gLoadGen{0}; /* increments each spawn; clears stuck loading */
	std::atomic<bool> gLoading{false};
	std::atomic<bool> gForceReload{false};
	std::atomic<bool> gIndexStarted{false};
	/* Bumped whenever gEnabledPaths changes. LoadMapTrails records the gen it
	   applied; Update retries until they match — a bool force-flag alone could
	   be cleared before a failed load, leaving trails empty until Reload packs. */
	std::atomic<uint32_t> gEnabledGen{1};
	uint32_t gLoadedEnabledGen = 0; /* under gMutex; 0 = never applied */
	std::atomic<int> gPackCount{0};
	std::vector<std::string> gPackNames; /* loaded .taco basenames (under gMutex) */
	std::thread gWorker;

	std::atomic<HINTERNET> gLiveSession{nullptr};
	std::atomic<HINTERNET> gLiveRequest{nullptr};




	std::vector<IndexedTrail> gIndex;
	std::vector<IndexedPoi> gPoiIndex;
	std::unordered_map<std::string, MarkerStyle> gCategoryStyles;
	std::vector<TekkitTrails::Category> gMenu; /* Tekkit overlay menu order */
	std::atomic<uint64_t> gMenuRevision{1};
	std::atomic<uint64_t> gContentRevision{1};
	uint32_t gActiveMap = 0;
	std::vector<TekkitTrails::Trail> gCurrentAll; /* all trails for map */
	std::vector<TekkitTrails::Marker> gCurrentMarkers;
	/* Opt-in: empty = nothing draws. Enabling a path shows that category and
	   all descendants (TacO-style prefix). */ 
	std::vector<std::string> gEnabledPaths;

	struct PendingIcon
	{
		std::string id;
		std::vector<uint8_t> bytes;
	};
	std::mutex gIconMutex;
	std::vector<PendingIcon> gPendingIcons;
	std::unordered_map<std::string, bool> gIconQueued; /* iconFile → queued */
	bool gGuideActive = false;
	float gGuideDestX = 0.f;
	float gGuideDestY = 0.f;
	TekkitTrails::Trail gGuide{};

	std::unordered_map<uint32_t, bool> gMapRectsReady;
	struct Rects
	{
		float mx0 = 0, my0 = 0, mx1 = 1, my1 = 1;
		float cx0 = 0, cy0 = 0, cx1 = 1, cy1 = 1;
		bool valid = false;
	};
	std::unordered_map<uint32_t, Rects> gRects;

	void CloseHttpHandle(std::atomic<HINTERNET>& slot)
	{
		HINTERNET h = slot.exchange(nullptr, std::memory_order_acq_rel);
		if (h)
			WinHttpCloseHandle(h);
	}

	void AbortHttp()
	{
		CloseHttpHandle(gLiveRequest);
		CloseHttpHandle(gLiveSession);
	}

	bool HttpGet(const std::wstring& host, const std::wstring& path, std::string& out,
		size_t maxBytes = 512 * 1024, int timeoutMs = 2500)
	{
		out.clear();
		timeoutMs = std::min(timeoutMs, 4000);
		HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/1.40",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session)
			return false;
		WinHttpSetTimeouts(session, 800, 800, 1500, timeoutMs);
		gLiveSession.store(session, std::memory_order_release);

		HINTERNET connect = WinHttpConnect(session, host.c_str(),
			INTERNET_DEFAULT_HTTPS_PORT, 0);
		if (!connect)
		{
			CloseHttpHandle(gLiveSession);
			return false;
		}
		HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(),
			nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
			WINHTTP_FLAG_SECURE);
		if (!request)
		{
			WinHttpCloseHandle(connect);
			CloseHttpHandle(gLiveSession);
			return false;
		}
		gLiveRequest.store(request, std::memory_order_release);

		bool ok = false;
		if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
				WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
			WinHttpReceiveResponse(request, nullptr))
		{
			DWORD size = 0;
			do
			{
				if (gLiveRequest.load(std::memory_order_acquire) == nullptr)
				{
					out.clear();
					ok = false;
					break;
				}
				size = 0;
				if (!WinHttpQueryDataAvailable(request, &size) || size == 0)
					break;
				if (out.size() + size > maxBytes)
					break;
				std::string chunk(size, '\0');
				DWORD read = 0;
				if (!WinHttpReadData(request, chunk.data(), size, &read))
					break;
				chunk.resize(read);
				out.append(chunk);
			} while (size > 0);
			ok = !out.empty();
		}

		HINTERNET ownedReq = gLiveRequest.exchange(nullptr, std::memory_order_acq_rel);
		if (ownedReq)
			WinHttpCloseHandle(ownedReq);
		WinHttpCloseHandle(connect);
		HINTERNET ownedSes = gLiveSession.exchange(nullptr, std::memory_order_acq_rel);
		if (ownedSes)
			WinHttpCloseHandle(ownedSes);
		return ok;
	}

	bool FetchMapRects(uint32_t mapId, Rects& r)
	{
		wchar_t path[64];
		std::swprintf(path, 64, L"/v2/maps/%u", mapId);
		std::string json;
		if (!HttpGet(L"api.guildwars2.com", path, json, 256 * 1024, 3000))
			return false;

		/* map_rect:[[x0,y0],[x1,y1]] continent_rect:[[x0,y0],[x1,y1]] */
		auto findRect = [&](const char* key, float& a, float& b, float& c, float& d) -> bool
		{
			const std::string needle = std::string("\"") + key + "\"";
			size_t p = json.find(needle);
			if (p == std::string::npos)
				return false;
			p = json.find('[', p);
			if (p == std::string::npos)
				return false;
			double v[4]{};
			char* end = nullptr;
			const char* s = json.c_str() + p;
			int n = 0;
			while (*s && n < 4)
			{
				if (*s == '[' || *s == ',' || *s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')
				{
					++s;
					continue;
				}
				if (*s == ']')
				{
					++s;
					continue;
				}
				v[n] = std::strtod(s, &end);
				if (end == s)
					break;
				s = end;
				++n;
			}
			if (n < 4)
				return false;
			a = static_cast<float>(v[0]);
			b = static_cast<float>(v[1]);
			c = static_cast<float>(v[2]);
			d = static_cast<float>(v[3]);
			return true;
		};

		if (!findRect("map_rect", r.mx0, r.my0, r.mx1, r.my1))
			return false;
		if (!findRect("continent_rect", r.cx0, r.cy0, r.cx1, r.cy1))
			return false;
		if (!(r.mx1 > r.mx0 && r.my1 > r.my0 && r.cx1 != r.cx0 && r.cy1 != r.cy0))
			return false;
		r.valid = true;
		return true;
	}

	void WorldToContinent(const Rects& r, float wxMeters, float wzMeters, float& cx, float& cy)
	{
		/* TacO / Blish / Mumble store world XZ in meters. API map_rect is in
		   inches (GW2 internal units). Without this scale every trail collapses
		   to a few pixels near the map center — the "blob" bug. */
		constexpr float kMetersToInches = 39.3700787f;
		const float wx = wxMeters * kMetersToInches;
		const float wz = wzMeters * kMetersToInches;

		const float tx = (wx - r.mx0) / (r.mx1 - r.mx0);
		/* Same transform as the classic Mumble→continent formula:
		   continent_y uses -world_z against map_rect.y. */
		const float ty = (-wz - r.my0) / (r.my1 - r.my0);
		cx = r.cx0 + tx * (r.cx1 - r.cx0);
		cy = r.cy0 + ty * (r.cy1 - r.cy0);
	}


	bool PrefixMatchesType(const std::string& typeLow, const std::string& prefixLow)
	{
		if (prefixLow.empty() || typeLow.empty())
			return false;
		if (typeLow == prefixLow)
			return true;
		return typeLow.size() > prefixLow.size() &&
			typeLow.compare(0, prefixLow.size(), prefixLow) == 0 &&
			typeLow[prefixLow.size()] == '.';
	}




	std::string IconTextureId(const std::string& iconFile)
	{
		std::string id = "TW_ICO_";
		for (char c : iconFile)
		{
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9'))
				id += c;
			else
				id += '_';
		}
		if (id.size() > 120)
			id.resize(120);
		return id;
	}

	void MergeCategoryTree(std::vector<TekkitTrails::Category>& dest, TekkitTrails::Category&& src);
	void MarkEnabled(std::vector<TekkitTrails::Category>& nodes);

	void IndexPack(const std::wstring& packPath, std::vector<IndexedTrail>& out,
		std::vector<IndexedPoi>& poisOut, std::vector<TekkitTrails::Category>& menuOut,
		std::unordered_map<std::string, MarkerStyle>& stylesOut,
		uint32_t epoch)
	{
		std::vector<uint8_t> file;
		if (!ReadFileW(packPath, file, kMaxZipBytes))
			return;

		mz_zip_archive zip{};
		if (!mz_zip_reader_init_mem(&zip, file.data(), file.size(), 0))
			return;

		const size_t startIdx = out.size();
		const int n = static_cast<int>(mz_zip_reader_get_num_files(&zip));
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
			IndexXml(packPath, xml, out);
			IndexPoisXml(packPath, xml, poisOut);

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
			add(addons + L"\\GW2-InGame-Helper-Beta\\pathing");
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

		/* Prefer our DLL path (…/addons/GW2-InGame-Helper-Beta[/].dll) — reliable under Wine. */
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
					if (_wcsicmp(leaf.c_str(), L"GW2-InGame-Helper-Beta") == 0 ||
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


	/* Merge a parsed category subtree into dest by path (official overlay depth). */
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
		if (!src.separator)
			found->separator = false;
		for (TekkitTrails::Category& ch : src.children)
			MergeCategoryTree(found->children, std::move(ch));
		src.children.clear();
	}

	/* (Prune removed — keep every Tekkit MarkerCategory toggle.) */

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

	bool TypeEnabledWithEnabled(const std::string& type, const std::vector<std::string>& enabled)
	{
		if (type.empty() || enabled.empty())
			return false;
		const std::string typeLow = ToLower(type);
		for (const std::string& p : enabled)
		{
			if (PrefixMatchesType(typeLow, ToLower(p)))
				return true;
		}
		return false;
	}

	bool TypeEnabledLocked(const std::string& type)
	{
		return TypeEnabledWithEnabled(type, gEnabledPaths);
	}

	bool CategoryUiEnabledLocked(const std::string& path)
	{
		if (path.empty() || gEnabledPaths.empty())
			return false;
		const std::string low = ToLower(path);
		for (const std::string& p : gEnabledPaths)
		{
			const std::string el = ToLower(p);
			/* Exact match, or an enabled ancestor covers this node. */
			if (PrefixMatchesType(low, el))
				return true;
		}
		return false;
	}

	float Dist2(float ax, float ay, float bx, float by)
	{
		const float dx = ax - bx;
		const float dy = ay - by;
		return dx * dx + dy * dy;
	}

	float gGuidePlayerX = 0.f;
	float gGuidePlayerY = 0.f;
	bool  gGuideHavePlayer = false;

	void RebuildSearchGuideLocked()
	{
		gGuide = {};
		if (!gGuideActive || gCurrentAll.empty())
			return;

		const float destX = gGuideDestX;
		const float destY = gGuideDestY;
		const bool havePlayer = gGuideHavePlayer;
		const float playerX = havePlayer ? gGuidePlayerX : destX;
		const float playerY = havePlayer ? gGuidePlayerY : destY;

		/* Continent units — allow a long snap radius so WP search can latch onto
		   Tekkit / Lady Elyssa trails that don't pass exactly through the WP. */
		constexpr float kMaxDestDist2 = 6000.f * 6000.f;
		constexpr float kMaxPlayerDist2 = 8000.f * 8000.f;

		int bestTrail = -1;
		int bestPi = 0;
		int bestDi = 0;
		float bestScore = 1e30f;

		for (size_t t = 0; t < gCurrentAll.size(); ++t)
		{
			const TekkitTrails::Trail& tr = gCurrentAll[t];
			if (tr.points.size() < 2)
				continue;

			int di = 0;
			int pi = 0;
			float bestD = 1e30f;
			float bestP = 1e30f;
			for (size_t i = 0; i < tr.points.size(); ++i)
			{
				const float dD = Dist2(tr.points[i].x, tr.points[i].y, destX, destY);
				if (dD < bestD)
				{
					bestD = dD;
					di = static_cast<int>(i);
				}
				if (havePlayer)
				{
					const float dP = Dist2(tr.points[i].x, tr.points[i].y, playerX, playerY);
					if (dP < bestP)
					{
						bestP = dP;
						pi = static_cast<int>(i);
					}
				}
			}
			if (bestD > kMaxDestDist2)
				continue;
			if (havePlayer && bestP > kMaxPlayerDist2)
				continue;

			const float score = havePlayer ? (bestD + bestP * 0.85f) : bestD;
			if (score < bestScore)
			{
				bestScore = score;
				bestTrail = static_cast<int>(t);
				bestDi = di;
				bestPi = havePlayer ? pi : 0;
			}
		}

		if (bestTrail < 0)
			return;

		const TekkitTrails::Trail& src = gCurrentAll[static_cast<size_t>(bestTrail)];
		int a = bestPi;
		int b = bestDi;
		if (a > b)
			std::swap(a, b);
		a = std::max(0, a - 1);
		b = std::min(static_cast<int>(src.points.size()) - 1, b + 1);
		if (b - a < 1)
			return;

		gGuide.mapId = src.mapId;
		gGuide.color = 0xFFFFAA20u;
		std::snprintf(gGuide.label, sizeof(gGuide.label), "Search route · %s", src.label);
		gGuide.points.assign(src.points.begin() + a, src.points.begin() + b + 1);
		/* Keep world meters in lockstep — in-world GPS needs these. */
		if (src.worldPoints.size() == src.points.size())
		{
			gGuide.worldPoints.assign(
				src.worldPoints.begin() + a, src.worldPoints.begin() + b + 1);
		}
		if (bestPi > bestDi)
		{
			std::reverse(gGuide.points.begin(), gGuide.points.end());
			std::reverse(gGuide.worldPoints.begin(), gGuide.worldPoints.end());
		}
	}

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
			if (ok)
				return true;
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

	void LoadMapTrails(uint32_t mapId, uint32_t epoch)
	{
		std::vector<IndexedTrail> indexCopy;
		std::vector<IndexedPoi> poiCopy;
		std::vector<std::string> enabledCopy;
		std::unordered_map<std::string, MarkerStyle> styleCopy;
		uint32_t enabledGen = 0;
		{
			std::lock_guard<std::mutex> lock(gMutex);
			indexCopy = gIndex;
			poiCopy = gPoiIndex;
			enabledCopy = gEnabledPaths;
			styleCopy = gCategoryStyles;
			enabledGen = gEnabledGen.load(std::memory_order_acquire);
		}

		/* Nothing opted in and no active search → skip opening the ~100MB pack.
		   This was a common Wine OOM path when every category defaulted on. */
		const bool needPack = !enabledCopy.empty() || gGuideActive;
		if (!needPack)
		{
			std::lock_guard<std::mutex> lock(gMutex);
			if (gEpoch.load(std::memory_order_acquire) != epoch)
				return;
			gActiveMap = mapId;
			gLoadedEnabledGen = enabledGen;
			gCurrentAll.clear();
			gCurrentMarkers.clear();
			gGuide = {};
			gContentRevision.fetch_add(1, std::memory_order_release);
			return;
		}

		Rects rects{};
		{
			std::lock_guard<std::mutex> lock(gMutex);
			auto it = gRects.find(mapId);
			if (it != gRects.end() && it->second.valid)
				rects = it->second;
		}
		if (!rects.valid)
		{
			if (!FetchMapRects(mapId, rects) || gEpoch.load(std::memory_order_acquire) != epoch)
			{
				/* Stop Update from hammering a failing fetch every frame. */
				std::lock_guard<std::mutex> lock(gMutex);
				if (gEpoch.load(std::memory_order_acquire) == epoch)
				{
					gActiveMap = mapId;
					gLoadedEnabledGen = enabledGen;
				}
				return;
			}
			std::lock_guard<std::mutex> lock(gMutex);
			gRects[mapId] = rects;
		}

		/* Only trails for this map (mapId resolved at index time). Rank:
		   0 = category-enabled (drawn), 1 = map-completion, 2 = other on-map
		   (available for search routing). Group by pack so we open each big
		   zip at most once and hold only one in memory at a time. */
		struct Cand
		{
			const IndexedTrail* it;
			int rank;
		};
		std::vector<Cand> cands;
		cands.reserve(256);
		for (const IndexedTrail& it : indexCopy)
		{
			if (it.mapId != mapId)
				continue; /* header-resolved; unknown (0) trails are skipped */
			int rank;
			if (TypeEnabledWithEnabled(it.type, enabledCopy))
				rank = 0;
			else if (it.mapCompletion)
				rank = 1;
			else
				rank = 2;
			cands.push_back({&it, rank});
		}

		/* Stable sort by pack (fewer reopens) then rank. */
		std::stable_sort(cands.begin(), cands.end(),
			[](const Cand& a, const Cand& b) {
				if (a.it->packPath != b.it->packPath)
					return a.it->packPath < b.it->packPath;
				return a.rank < b.rank;
			});

		std::vector<TekkitTrails::Trail> loaded;
		loaded.reserve(64);
		std::unordered_map<std::string, std::wstring> assetsNeeded;
		size_t otherCount = 0;

		OpenPack pack;
		std::wstring openPath;

		for (const Cand& c : cands)
		{
			if (gEpoch.load(std::memory_order_acquire) != epoch)
				return;
			if (loaded.size() >= kMaxTrailsPerMap)
				break;
			/* Cap non-enabled trails kept only for search routing. */
			if (c.rank != 0 && otherCount >= 80)
				continue;

			const IndexedTrail& it = *c.it;
			if (it.packPath != openPath)
			{
				pack.Close();
				openPath = it.packPath;
				if (!pack.Open(openPath))
					continue;
			}

			std::vector<uint8_t> bytes;
			const int fi = (it.fileIndex >= 0) ? it.fileIndex
				: ZipLocate(pack.zip, it.entryName);
			if (!ZipExtractIndex(pack.zip, fi, bytes, kMaxTrailFile))
				continue;

			uint32_t trailMap = 0;
			std::vector<TekkitTrails::WorldPoint> world;
			if (!ParseTrl(bytes, trailMap, world) || trailMap != mapId)
				continue;

			TekkitTrails::Trail trail{};
			const MarkerStyle style = ResolveStyle(it.type, it.style, styleCopy);
			trail.mapId = mapId;
			trail.color = style.hasColor ? style.color : it.color;
			trail.minimapVisible = style.minimapVisible;
			trail.inGameVisible = style.inGameVisible;
			trail.alpha = std::clamp(style.alpha, 0.f, 1.f);
			trail.trailScale = std::clamp(style.trailScale, 0.1f, 8.f);
			trail.fadeNear = style.fadeNear;
			trail.fadeFar = style.fadeFar;
			if (style.hasTexture && !style.texture.empty())
			{
				const std::string tid = IconTextureId(style.texture);
				std::snprintf(trail.textureId, sizeof(trail.textureId), "%s", tid.c_str());
				if (c.rank == 0)
					assetsNeeded.emplace(style.texture, it.packPath);
			}
			std::snprintf(trail.label, sizeof(trail.label), "%s",
				it.type.empty() ? "trail" : it.type.c_str());
			trail.points.reserve(world.size());
			for (const TekkitTrails::WorldPoint& w : world)
			{
				TekkitTrails::Point cc{};
				WorldToContinent(rects, w.x, w.z, cc.x, cc.y);
				if (!std::isfinite(cc.x) || !std::isfinite(cc.y))
					continue;
				trail.points.push_back(cc);
			}
			/* Keep full-span world samples for enabled trails (ParseTrl already
			   covers start→end). Do not re-cap from the front. */
			if (c.rank == 0 && trail.points.size() >= 2)
				trail.worldPoints = std::move(world);
			if (trail.points.size() < 2)
				continue;
			if (c.rank != 0)
				++otherCount;
			loaded.push_back(std::move(trail));
		}

		/* POI markers for this map — same TacO prefix enable rules as trails. */
		std::vector<TekkitTrails::Marker> markers;
		markers.reserve(std::min(poiCopy.size(), kMaxMarkersPerMap));
		for (const IndexedPoi& poi : poiCopy)
		{
			if (poi.mapId != mapId)
				continue;
			if (!TypeEnabledWithEnabled(poi.type, enabledCopy))
				continue;
			const MarkerStyle style = ResolveStyle(poi.type, poi.style, styleCopy);
			TekkitTrails::Point cc{};
			WorldToContinent(rects, poi.wx, poi.wz, cc.x, cc.y);
			if (!std::isfinite(cc.x) || !std::isfinite(cc.y))
				continue;
			TekkitTrails::Marker m{};
			m.mapId = mapId;
			m.color = style.hasColor ? style.color : 0xFFFFCC33u;
			m.pos = cc;
			m.world = {poi.wx, poi.wy, poi.wz};
			m.minimapVisible = style.minimapVisible;
			m.inGameVisible = style.inGameVisible;
			m.mapDisplaySize = std::max(1.f, style.mapDisplaySize);
			m.minSize = std::max(1.f, style.minSize);
			m.maxSize = std::max(m.minSize, style.maxSize);
			m.iconSize = std::max(0.05f, style.iconSize);
			m.heightOffset = style.heightOffset;
			m.fadeNear = style.fadeNear;
			m.fadeFar = style.fadeFar;
			m.alpha = std::clamp(style.alpha, 0.f, 1.f);
			std::snprintf(m.label, sizeof(m.label), "%s", poi.type.c_str());
			const std::string& icon = style.iconFile;
			if (!icon.empty())
			{
				const std::string tid = IconTextureId(icon);
				std::snprintf(m.iconId, sizeof(m.iconId), "%s", tid.c_str());
				assetsNeeded.emplace(icon, poi.packPath);
			}
			markers.push_back(std::move(m));
			if (markers.size() >= kMaxMarkersPerMap)
				break;
		}

		/* Extract a bounded set of icons while opening each huge pack only once.
		   v62 reopened Tekkit's ~100MB zip per icon, causing long stalls/OOM. */
		if (!assetsNeeded.empty())
		{
			std::vector<std::pair<std::string, std::wstring>> iconList(
				assetsNeeded.begin(), assetsNeeded.end());
			std::sort(iconList.begin(), iconList.end(),
				[](const auto& a, const auto& b) { return a.second < b.second; });
			OpenPack iconPack;
			std::wstring iconPackPath;
			size_t queued = 0;
			for (const auto& kv : iconList)
			{
				if (queued >= 128 || gEpoch.load(std::memory_order_acquire) != epoch)
					break;
				{
					std::lock_guard<std::mutex> lock(gIconMutex);
					if (gIconQueued.count(kv.first))
						continue;
				}
				if (kv.second != iconPackPath)
				{
					iconPack.Close();
					iconPackPath = kv.second;
					if (!iconPack.Open(iconPackPath))
						continue;
				}
				std::string entry = kv.first;
				std::replace(entry.begin(), entry.end(), '\\', '/');
				while (entry.rfind("./", 0) == 0)
					entry.erase(0, 2);
				while (!entry.empty() && entry.front() == '/')
					entry.erase(entry.begin());
				int idx = ZipLocate(iconPack.zip, entry);
				if (idx < 0 && entry.rfind("Data/", 0) == 0)
					idx = ZipLocate(iconPack.zip, entry.substr(5));
				if (idx < 0)
					idx = ZipLocate(iconPack.zip, std::string("Data/") + entry);
				std::vector<uint8_t> bytes;
				if (idx < 0 || !ZipExtractIndex(iconPack.zip, idx, bytes, 2u * 1024u * 1024u))
					continue;
				PendingIcon pending;
				pending.id = IconTextureId(kv.first);
				pending.bytes = std::move(bytes);
				{
					std::lock_guard<std::mutex> lock(gIconMutex);
					if (gPendingIcons.size() >= 256)
						break;
					gIconQueued[kv.first] = true;
					gPendingIcons.push_back(std::move(pending));
				}
				++queued;
			}
		}

		std::lock_guard<std::mutex> lock(gMutex);
		if (gEpoch.load(std::memory_order_acquire) != epoch)
			return;
		gActiveMap = mapId;
		/* Always settle the gen we intended — perpetual mismatch was reloading
		   the pack every ~1s and blanking GPS. Another toggle bumps gen again. */
		gLoadedEnabledGen = gEnabledGen.load(std::memory_order_acquire);
		gCurrentAll = std::move(loaded);
		gCurrentMarkers = std::move(markers);
		gContentRevision.fetch_add(1, std::memory_order_release);
		RebuildSearchGuideLocked();
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
			}
			gPackCount.store(0, std::memory_order_release);

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

			/* Soft cap — huge multi-pack dumps blow memory under Wine. */
			constexpr size_t kMaxPacks = 8;
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
				IndexPack(pack, index, pois, menu, categoryStyles, epoch);
				if (index.size() + pois.size() > before)
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

	void InsertCatPath(std::vector<TekkitTrails::Category>& roots, const std::string& type)
	{
		if (type.empty())
			return;
		std::vector<std::string> parts;
		size_t start = 0;
		while (start < type.size() && parts.size() < 16)
		{
			size_t dot = type.find('.', start);
			if (dot == std::string::npos)
			{
				parts.push_back(type.substr(start));
				break;
			}
			parts.push_back(type.substr(start, dot - start));
			start = dot + 1;
		}
		if (parts.empty())
			return;

		std::vector<TekkitTrails::Category>* level = &roots;
		std::string path;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			if (!path.empty())
				path += '.';
			path += parts[i];
			TekkitTrails::Category* found = nullptr;
			for (TekkitTrails::Category& c : *level)
			{
				if (c.path == path)
				{
					found = &c;
					break;
				}
			}
			if (!found)
			{
				TekkitTrails::Category neu;
				neu.path = path;
				neu.label = parts[i];
				neu.trails = 0;
				neu.enabled = false;
				level->push_back(std::move(neu));
				found = &level->back();
			}
			++found->trails;
			level = &found->children;
		}
	}

	void MarkEnabled(std::vector<TekkitTrails::Category>& nodes)
	{
		for (TekkitTrails::Category& c : nodes)
		{
			c.enabled = CategoryUiEnabledLocked(c.path);
			MarkEnabled(c.children);
		}
	}
}

void TekkitTrails::Init()
{
	gEpoch.fetch_add(1, std::memory_order_acq_rel);
	gLoadGen.fetch_add(1, std::memory_order_acq_rel);
	AbortHttp();
	if (gWorker.joinable())
		gWorker.detach();
	gIndexStarted.store(false, std::memory_order_release);
	gLoading.store(false, std::memory_order_release);
	gForceReload.store(false, std::memory_order_release);
	gEnabledGen.store(1, std::memory_order_release);
	{
		std::lock_guard<std::mutex> lock(gMutex);
		gIndex.clear();
		gPoiIndex.clear();
		gCategoryStyles.clear();
		gMenu.clear();
		gCurrentAll.clear();
		gCurrentMarkers.clear();
		gGuide = {};
		gGuideActive = false;
		gActiveMap = 0;
		gLoadedEnabledGen = 0;
		/* Keep gEnabledPaths — Blish/TacO remember category toggles across reloads. */
		gPackNames.clear();
	}
	{
		std::lock_guard<std::mutex> lock(gIconMutex);
		gPendingIcons.clear();
		gIconQueued.clear();
	}
	gPackCount.store(0, std::memory_order_release);
}

void TekkitTrails::Shutdown()
{
	gEpoch.fetch_add(1, std::memory_order_acq_rel);
	gLoadGen.fetch_add(1, std::memory_order_acq_rel);
	AbortHttp();
	if (gWorker.joinable())
		gWorker.detach();
	{
		std::lock_guard<std::mutex> lock(gIconMutex);
		gPendingIcons.clear();
		gIconQueued.clear();
	}
	std::lock_guard<std::mutex> lock(gMutex);
	gIndex.clear();
	gPoiIndex.clear();
	gCategoryStyles.clear();
	gMenu.clear();
	gCurrentAll.clear();
	gCurrentMarkers.clear();
	gGuide = {};
	gGuideActive = false;
	gLoading.store(false, std::memory_order_release);
}

void TekkitTrails::Update(uint32_t mapId)
{
	if (!G::ShowTekkitTrails || mapId == 0)
		return;

	/* Keep search routing locked to the live player continent position, but only
	   rebuild when the player has actually moved a bit — rebuilding scans every
	   trail on the map, so doing it every frame would stutter. */
	if (G::Mumble)
	{
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		if (ctx && ctx->mapId != 0)
		{
			std::lock_guard<std::mutex> lock(gMutex);
			const float dx = ctx->playerX - gGuidePlayerX;
			const float dy = ctx->playerY - gGuidePlayerY;
			const bool moved = (dx * dx + dy * dy) > (120.f * 120.f);
			gGuidePlayerX = ctx->playerX;
			gGuidePlayerY = ctx->playerY;
			const bool first = !gGuideHavePlayer;
			gGuideHavePlayer = true;
			if (gGuideActive && (moved || first))
				RebuildSearchGuideLocked();
		}
	}

	if (!gIndexStarted.load(std::memory_order_acquire))
	{
		gIndexStarted.store(true, std::memory_order_release);
		const uint32_t epoch = gEpoch.fetch_add(1, std::memory_order_acq_rel) + 1;
		const uint32_t loadGen = gLoadGen.fetch_add(1, std::memory_order_acq_rel) + 1;
		gLoading.store(true, std::memory_order_release);
		if (gWorker.joinable())
			gWorker.detach();
		gWorker = std::thread([epoch, loadGen, mapId]() {
			struct Guard
			{
				uint32_t loadGen = 0;
				~Guard()
				{
					if (gLoadGen.load(std::memory_order_acquire) == loadGen)
						gLoading.store(false, std::memory_order_release);
				}
			} guard{loadGen};
			WorkerLoop(epoch, mapId);
		});
		return;
	}

	if (gLoading.load(std::memory_order_acquire))
		return;

	uint32_t active = 0;
	uint32_t loadedGen = 0;
	{
		std::lock_guard<std::mutex> lock(gMutex);
		active = gActiveMap;
		loadedGen = gLoadedEnabledGen;
	}
	const uint32_t wantGen = gEnabledGen.load(std::memory_order_acquire);
	const bool force = gForceReload.exchange(false, std::memory_order_acq_rel);

	if (mapId == active && wantGen == loadedGen && !force)
		return;

	/* Invalidate any still-running detached LoadMapTrails before spawning. */
	const uint32_t epoch = gEpoch.fetch_add(1, std::memory_order_acq_rel) + 1;
	const uint32_t loadGen = gLoadGen.fetch_add(1, std::memory_order_acq_rel) + 1;
	gLoading.store(true, std::memory_order_release);
	if (gWorker.joinable())
		gWorker.detach();
	gWorker = std::thread([epoch, loadGen, mapId]() {
		struct Guard
		{
			uint32_t loadGen = 0;
			~Guard()
			{
				if (gLoadGen.load(std::memory_order_acquire) == loadGen)
					gLoading.store(false, std::memory_order_release);
			}
		} guard{loadGen};
		try
		{
			LoadMapTrails(mapId, epoch);
		}
		catch (...)
		{
		}
	});
}

bool TekkitTrails::MasterEnabled() { return G::ShowTekkitTrails; }
void TekkitTrails::SetMasterEnabled(bool on) { G::ShowTekkitTrails = on; }

bool TekkitTrails::IsLoading() { return gLoading.load(std::memory_order_acquire); }
int TekkitTrails::PackCount() { return gPackCount.load(std::memory_order_acquire); }

std::vector<std::string> TekkitTrails::LoadedPackNames()
{
	std::lock_guard<std::mutex> lock(gMutex);
	return gPackNames;
}

std::string TekkitTrails::PathingFolderHint()
{
	if (G::API && G::API->Paths_GetAddonDirectory)
	{
		const char* ad = G::API->Paths_GetAddonDirectory(ADDON_NAME);
		if (ad && ad[0])
		{
			std::string p(ad);
			while (!p.empty() && (p.back() == '\\' || p.back() == '/'))
				p.pop_back();
			return p + "\\pathing";
		}
	}
	return "addons\\GW2-InGame-Helper-Beta\\pathing";
}

void TekkitTrails::ReloadPacks()
{
	/* Invalidate in-flight work and ask Update() to re-index. Do NOT clear
	   multi-MB trail/index vectors here — that ran on the UI thread and froze
	   the game under Wine. The worker clears and rebuilds. */
	gEpoch.fetch_add(1, std::memory_order_acq_rel);
	gLoadGen.fetch_add(1, std::memory_order_acq_rel);
	AbortHttp();
	if (gWorker.joinable())
		gWorker.detach();
	{
		std::lock_guard<std::mutex> lock(gMutex);
		gActiveMap = 0;
		gLoadedEnabledGen = 0;
	}
	gLoading.store(false, std::memory_order_release);
	gForceReload.store(false, std::memory_order_release);
	gIndexStarted.store(false, std::memory_order_release);
}

uint64_t TekkitTrails::ContentRevision()
{
	return gContentRevision.load(std::memory_order_acquire);
}

int TekkitTrails::TrailCountAllOnMap()
{
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return 0;
	return static_cast<int>(gCurrentAll.size());
}

int TekkitTrails::TrailCount()
{
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return 0;
	int n = 0;
	for (const Trail& t : gCurrentAll)
	{
		if (TypeEnabledLocked(t.label))
			++n;
	}
	return n;
}

std::vector<TekkitTrails::Trail> TekkitTrails::CurrentTrails()
{
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return {};
	std::vector<Trail> out;
	/* Minimap only needs continent polylines — never deep-copy worldPoints. */
	constexpr int kMaxDraw = 96;
	out.reserve(static_cast<size_t>(kMaxDraw));
	for (const Trail& t : gCurrentAll)
	{
		if (static_cast<int>(out.size()) >= kMaxDraw)
			break;
		if (!TypeEnabledLocked(t.label) || !t.minimapVisible || t.points.size() < 2)
			continue;
		Trail slim{};
		slim.mapId = t.mapId;
		slim.color = t.color;
		std::snprintf(slim.textureId, sizeof(slim.textureId), "%s", t.textureId);
		slim.minimapVisible = t.minimapVisible;
		slim.inGameVisible = t.inGameVisible;
		slim.alpha = t.alpha;
		slim.trailScale = t.trailScale;
		slim.fadeNear = t.fadeNear;
		slim.fadeFar = t.fadeFar;
		std::snprintf(slim.label, sizeof(slim.label), "%s", t.label);
		slim.points = t.points;
		out.push_back(std::move(slim));
	}
	return out;
}

int TekkitTrails::MarkerCount()
{
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return 0;
	return static_cast<int>(gCurrentMarkers.size());
}

std::vector<TekkitTrails::Marker> TekkitTrails::CurrentMarkers()
{
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return {};
	return gCurrentMarkers;
}

std::vector<TekkitTrails::Marker> TekkitTrails::CurrentMarkersInBounds(
	float minX, float minY, float maxX, float maxY)
{
	std::vector<Marker> out;
	if (!(minX <= maxX && minY <= maxY))
		return out;
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return out;
	out.reserve(std::min<size_t>(gCurrentMarkers.size(), kMaxMinimapMarkers));
	for (const Marker& marker : gCurrentMarkers)
	{
		if (!marker.minimapVisible)
			continue;
		if (marker.pos.x < minX || marker.pos.x > maxX ||
			marker.pos.y < minY || marker.pos.y > maxY)
			continue;
		out.push_back(marker);
		if (out.size() >= kMaxMinimapMarkers)
			break;
	}
	return out;
}

std::vector<TekkitTrails::Marker> TekkitTrails::NearbyWorldMarkers(
	float x, float y, float z, float maxDistance, size_t maxMarkers)
{
	std::vector<Marker> out;
	if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
		maxDistance <= 0.f || maxMarkers == 0)
		return out;
	const float maxD2 = maxDistance * maxDistance;
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return out;
	out.reserve(std::min(maxMarkers, gCurrentMarkers.size()));
	for (const Marker& marker : gCurrentMarkers)
	{
		if (!marker.inGameVisible)
			continue;
		const float dx = marker.world.x - x;
		const float dy = marker.world.y - y;
		const float dz = marker.world.z - z;
		const float d2 = dx * dx + dy * dy + dz * dz;
		if (!std::isfinite(d2) || d2 > maxD2)
			continue;
		out.push_back(marker);
		if (out.size() >= maxMarkers)
			break;
	}
	return out;
}

void TekkitTrails::BeginFrame()
{
	if (!G::API || !G::API->Textures_GetOrCreateFromMemory)
		return;
	for (int n = 0; n < 4; ++n)
	{
		PendingIcon icon;
		{
			std::lock_guard<std::mutex> lock(gIconMutex);
			if (gPendingIcons.empty())
				return;
			icon = std::move(gPendingIcons.front());
			gPendingIcons.erase(gPendingIcons.begin());
		}
		if (icon.bytes.empty() || icon.id.empty())
			continue;
		if (G::API->Textures_Get(icon.id.c_str()) &&
			G::API->Textures_Get(icon.id.c_str())->Resource)
			continue;
		G::API->Textures_GetOrCreateFromMemory(
			icon.id.c_str(), icon.bytes.data(),
			static_cast<uint64_t>(icon.bytes.size()));
	}
}

std::vector<TekkitTrails::WorldSnippet> TekkitTrails::NearbyWorldSnippets(
	float avatarX, float avatarY, float avatarZ,
	float maxDistMeters, int maxTrails, int maxPointTests)
{
	std::vector<WorldSnippet> out;
	if (maxTrails < 1 || maxPointTests < 1)
		return out;
	/* Nearby slice only — short range so GPS does not paint through walls/map. */
	const float maxDist = std::clamp(maxDistMeters, 10.f, 120.f);
	const float softDist = maxDist * 1.35f;
	const float softDist2 = softDist * softDist;

	if (!std::isfinite(avatarX) || !std::isfinite(avatarY) || !std::isfinite(avatarZ))
		return out;

	auto dist2 = [&](float x, float y, float z) {
		if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
			return 1.0e30f;
		const float dx = avatarX - x;
		const float dy = avatarY - y;
		const float dz = avatarZ - z;
		const float d = dx * dx + dy * dy + dz * dz;
		return std::isfinite(d) ? d : 1.0e30f;
	};

	/* Never block the render thread — a held worker lock froze/crashed Wine. */
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return out;
	if (gCurrentAll.empty())
		return out;

	struct Cand
	{
		size_t idx = 0;
		size_t nearest = 0;
		float nearestD2 = 1.0e30f;
	};
	std::vector<Cand> cands;
	cands.reserve(32);

	int pointTests = 0;
	for (size_t ti = 0; ti < gCurrentAll.size(); ++ti)
	{
		const Trail& tr = gCurrentAll[ti];
		if (tr.worldPoints.size() < 2 || !TypeEnabledLocked(tr.label))
			continue;
		const size_t n = tr.worldPoints.size();
		size_t bestI = 0;
		float bestD = 1.0e30f;
		const size_t step = std::max<size_t>(1, n / 20);
		for (size_t i = 0; i < n; i += step)
		{
			if (++pointTests > maxPointTests)
				break;
			const WorldPoint& p = tr.worldPoints[i];
			const float d = dist2(p.x, p.y, p.z);
			if (d < bestD)
			{
				bestD = d;
				bestI = i;
			}
		}
		if (bestD <= softDist2)
			cands.push_back({ti, bestI, bestD});
		if (pointTests > maxPointTests)
			break;
	}

	std::sort(cands.begin(), cands.end(),
		[](const Cand& a, const Cand& b) { return a.nearestD2 < b.nearestD2; });

	out.reserve(static_cast<size_t>(std::min(maxTrails, 16)));
	for (const Cand& c : cands)
	{
		if (static_cast<int>(out.size()) >= maxTrails)
			break;
		const Trail& tr = gCurrentAll[c.idx];
		const auto& pts = tr.worldPoints;
		const size_t n = pts.size();
		size_t a = c.nearest;
		size_t b = c.nearest;
		constexpr size_t kMinPad = 12;
		if (a > kMinPad)
			a -= kMinPad;
		else
			a = 0;
		b = std::min(n - 1, b + kMinPad);
		while (a > 0 && dist2(pts[a - 1].x, pts[a - 1].y, pts[a - 1].z) <= softDist2)
		{
			--a;
			if (++pointTests > maxPointTests)
				break;
		}
		while (b + 1 < n && dist2(pts[b + 1].x, pts[b + 1].y, pts[b + 1].z) <= softDist2)
		{
			++b;
			if (++pointTests > maxPointTests)
				break;
		}
		if (b <= a)
			continue;

		WorldSnippet snip;
		snip.color = tr.color;
		std::snprintf(snip.textureId, sizeof(snip.textureId), "%s", tr.textureId);
		snip.alpha = tr.alpha;
		snip.trailScale = tr.trailScale;
		snip.fadeNear = tr.fadeNear;
		snip.fadeFar = tr.fadeFar;
		/* Local slice only — never ship a map-long ribbon to the render thread. */
		constexpr size_t kMaxPts = 96;
		snip.points.reserve(std::min(b - a + 1, kMaxPts));
		const size_t span = b - a;
		const size_t stride = (span > kMaxPts) ? (span / kMaxPts) : 1;
		for (size_t i = a; i <= b; i += std::max<size_t>(1, stride))
		{
			const WorldPoint& wp = pts[i];
			if (!std::isfinite(wp.x) || !std::isfinite(wp.y) || !std::isfinite(wp.z))
				continue;
			snip.points.push_back(wp);
			if (snip.points.size() >= kMaxPts)
				break;
		}
		if (snip.points.size() >= 2)
			out.push_back(std::move(snip));
		if (pointTests > maxPointTests)
			break;
	}
	return out;
}

bool TekkitTrails::TryNearbyWorldGps(
	float avatarX, float avatarY, float avatarZ, float maxDistMeters,
	std::vector<WorldSnippet>& outSnippets,
	std::vector<Marker>& outMarkers)
{
	outSnippets.clear();
	outMarkers.clear();
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return false;

	if (!std::isfinite(avatarX) || !std::isfinite(avatarY) || !std::isfinite(avatarZ))
		return true;

	/* Activation + draw: Tekkit-style distance fade around the player. The
	   polyline itself is the full trail (start→end); we only skip far segments. */
	const float maxDist = std::clamp(maxDistMeters, 20.f, 220.f);
	const float activateDist = std::max(maxDist * 2.0f, 180.f);
	const float activateDist2 = activateDist * activateDist;
	const float softDist = activateDist;
	const float softDist2 = softDist * softDist;
	auto dist2 = [&](float x, float y, float z) {
		const float dx = avatarX - x;
		const float dy = avatarY - y;
		const float dz = avatarZ - z;
		return dx * dx + dy * dy + dz * dz;
	};

	struct Cand
	{
		size_t idx = 0;
		size_t nearest = 0;
		float nearestD2 = 1.0e30f;
	};
	std::vector<Cand> cands;
	cands.reserve(32);
	int pointTests = 0;
	constexpr int kMaxPointTests = 8000;
	for (size_t ti = 0; ti < gCurrentAll.size(); ++ti)
	{
		const Trail& tr = gCurrentAll[ti];
		if (tr.worldPoints.size() < 2 || !TypeEnabledLocked(tr.label))
			continue;
		const size_t n = tr.worldPoints.size();
		size_t bestI = 0;
		float bestD = 1.0e30f;
		const size_t step = std::max<size_t>(1, n / 40);
		for (size_t i = 0; i < n; i += step)
		{
			if (++pointTests > kMaxPointTests)
				break;
			const float d = dist2(tr.worldPoints[i].x, tr.worldPoints[i].y, tr.worldPoints[i].z);
			if (d < bestD)
			{
				bestD = d;
				bestI = i;
			}
		}
		/* Refine near the coarse hit. */
		const size_t lo = bestI > step ? bestI - step : 0;
		const size_t hi = std::min(n, bestI + step + 1);
		for (size_t i = lo; i < hi; ++i)
		{
			const float d = dist2(tr.worldPoints[i].x, tr.worldPoints[i].y, tr.worldPoints[i].z);
			if (d < bestD)
			{
				bestD = d;
				bestI = i;
			}
		}
		if (bestD <= activateDist2)
			cands.push_back({ti, bestI, bestD});
		if (pointTests > kMaxPointTests)
			break;
	}
	std::sort(cands.begin(), cands.end(),
		[](const Cand& a, const Cand& b) { return a.nearestD2 < b.nearestD2; });

	outSnippets.reserve(std::min<size_t>(cands.size(), 20));
	for (const Cand& c : cands)
	{
		if (outSnippets.size() >= 20)
			break;
		const Trail& tr = gCurrentAll[c.idx];
		const auto& pts = tr.worldPoints;
		const size_t n = pts.size();
		size_t a = c.nearest;
		size_t b = c.nearest;
		/* Grow along the full polyline while near the player (Tekkit fade). */
		while (a > 0 && dist2(pts[a - 1].x, pts[a - 1].y, pts[a - 1].z) <= softDist2)
			--a;
		while (b + 1 < n && dist2(pts[b + 1].x, pts[b + 1].y, pts[b + 1].z) <= softDist2)
			++b;
		/* Keep a minimum ribbon even when samples are sparse. */
		constexpr size_t kPad = 24;
		if (c.nearest > kPad)
			a = std::min(a, c.nearest - kPad);
		else
			a = 0;
		b = std::max(b, std::min(n - 1, c.nearest + kPad));
		if (b <= a)
			continue;

		WorldSnippet snip;
		snip.color = tr.color;
		std::snprintf(snip.textureId, sizeof(snip.textureId), "%s", tr.textureId);
		snip.alpha = tr.alpha;
		snip.trailScale = tr.trailScale;
		snip.fadeNear = tr.fadeNear;
		snip.fadeFar = tr.fadeFar;
		/* Copy the local window densely — trail data already spans the full route. */
		constexpr size_t kMaxPts = 256;
		snip.points.reserve(std::min(b - a + 1, kMaxPts));
		const size_t span = b - a;
		const size_t stride = (span > kMaxPts) ? (span / kMaxPts) : 1;
		for (size_t i = a; i <= b; i += std::max<size_t>(1, stride))
		{
			const WorldPoint& wp = pts[i];
			if (!std::isfinite(wp.x) || !std::isfinite(wp.y) || !std::isfinite(wp.z))
				continue;
			snip.points.push_back(wp);
			if (snip.points.size() >= kMaxPts)
				break;
		}
		if (snip.points.size() >= 2)
			outSnippets.push_back(std::move(snip));
	}

	const float markDist2 = (activateDist * 1.2f) * (activateDist * 1.2f);
	outMarkers.reserve(64);
	for (const Marker& marker : gCurrentMarkers)
	{
		if (!marker.inGameVisible)
			continue;
		const float d = dist2(marker.world.x, marker.world.y, marker.world.z);
		if (d > markDist2)
			continue;
		outMarkers.push_back(marker);
		if (outMarkers.size() >= 120)
			break;
	}
	return true;
}

TekkitTrails::WorldSnippet TekkitTrails::SearchGuideWorldSnippet()
{
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	WorldSnippet snip;
	if (!lock.owns_lock())
		return snip;
	if (!gGuideActive || gGuide.worldPoints.size() < 2)
		return snip;
	snip.color = gGuide.color ? gGuide.color : 0xFFFFAA20u;
	snip.alpha = 1.f;
	snip.trailScale = 1.f;
	snip.points = gGuide.worldPoints;
	return snip;
}

std::vector<TekkitTrails::Category> TekkitTrails::CategoryTree()
{
	std::unique_lock<std::mutex> lock(gMutex, std::try_to_lock);
	if (!lock.owns_lock())
		return {};
	if (!gMenu.empty())
	{
		MarkEnabled(gMenu);
		return gMenu;
	}

	std::vector<Category> roots;
	for (const IndexedTrail& it : gIndex)
	{
		if (!it.type.empty())
			InsertCatPath(roots, it.type);
	}
	MarkEnabled(roots);
	return roots;
}

void TekkitTrails::SetCategoryEnabled(const std::string& path, bool enabled)
{
	std::lock_guard<std::mutex> lock(gMutex);
	const std::string low = ToLower(path);
	/* Drop this path, its descendants, and (when disabling) any ancestor that
	   was covering it via prefix inheritance. */
	gEnabledPaths.erase(
		std::remove_if(gEnabledPaths.begin(), gEnabledPaths.end(),
			[&](const std::string& p) {
				const std::string el = ToLower(p);
				return PrefixMatchesType(el, low) || PrefixMatchesType(low, el);
			}),
		gEnabledPaths.end());
	if (enabled)
		gEnabledPaths.push_back(path);
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

void TekkitTrails::EnableMapCompletionPreset(MapCompletionRoutes routes)
{
	if (routes == MapCompletionRoutes::None)
	{
		ClearMapCompletionCategories();
		return;
	}

	std::lock_guard<std::mutex> lock(gMutex);

	/* Match Core + every expansion (tw_mc_hot, tw_mc_pof, …) — PrefixMatchesType
	   on "tw_guides.tw_mc" alone misses tw_mc_hot because '_' ≠ '.'. */
	auto isMcPath = [](const std::string& p) -> bool
	{
		const std::string low = ToLower(p);
		if (low == "tw_guides.tw_mc")
			return true;
		if (low.size() >= 16 && low.compare(0, 16, "tw_guides.tw_mc.") == 0)
			return true;
		if (low.size() >= 16 && low.compare(0, 16, "tw_guides.tw_mc_") == 0)
			return true;
		return false;
	};

	/* Drop prior map-completion enables so we don't stack both editions. */
	gEnabledPaths.erase(
		std::remove_if(gEnabledPaths.begin(), gEnabledPaths.end(),
			[&](const std::string& p) { return isMcPath(p); }),
		gEnabledPaths.end());

	auto leafOf = [](const std::string& path) -> std::string
	{
		const size_t dot = path.find_last_of('.');
		return (dot == std::string::npos) ? path : path.substr(dot + 1);
	};

	auto isRouteFolder = [&](const Category& c) -> bool
	{
		const std::string leaf = ToLower(leafOf(c.path));
		const std::string lab = ToLower(c.label);
		if (leaf.find("trails") != std::string::npos)
			return true;
		if (lab.find("routes") != std::string::npos)
			return true;
		if (lab.find("edition") != std::string::npos)
			return true;
		return false;
	};

	/* Prefer DisplayName — SotO/VoE reuse trails/trails2 for Skyscale/Lanterns/Skimmer. */
	auto matchesRoutes = [&](const Category& c) -> bool
	{
		const std::string leaf = ToLower(leafOf(c.path));
		const std::string lab = ToLower(c.label);

		auto ends = [](const std::string& s, const char* suf) -> bool
		{
			const size_t n = std::strlen(suf);
			return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
		};

		/* Single generic "Routes" (e.g. Janthir Wilds) — OK for either pick. */
		if (lab == "routes")
			return true;

		const bool bare = lab.find("barefoot") != std::string::npos;
		const bool griff = lab.find("griffon") != std::string::npos;
		if (bare || griff)
			return (routes == MapCompletionRoutes::Barefoot) ? bare : griff;

		/* Fallback when DisplayName missing: Core/HoT/PoF/EoD/LWS naming only. */
		if (lab.find("skyscale") != std::string::npos ||
			lab.find("skimmer") != std::string::npos ||
			lab.find("lantern") != std::string::npos)
			return false;
		if (routes == MapCompletionRoutes::Barefoot)
			return ends(leaf, "trails2");
		return ends(leaf, "trails") && !ends(leaf, "trails2") && !ends(leaf, "trails3");
	};

	auto enablePath = [&](const std::string& path)
	{
		if (path.empty())
			return;
		const std::string low = ToLower(path);
		gEnabledPaths.erase(
			std::remove_if(gEnabledPaths.begin(), gEnabledPaths.end(),
				[&](const std::string& p) {
					const std::string el = ToLower(p);
					return PrefixMatchesType(el, low) || PrefixMatchesType(low, el);
				}),
			gEnabledPaths.end());
		gEnabledPaths.push_back(path);
	};

	std::function<void(const Category&)> visitExpansion = [&](const Category& node)
	{
		for (const Category& ch : node.children)
		{
			if (ch.hidden)
				continue;
			if (ch.separator)
			{
				visitExpansion(ch);
				continue;
			}
			if (isRouteFolder(ch))
			{
				if (matchesRoutes(ch))
					enablePath(ch.path);
				continue;
			}
			/* Hearts, POIs, vistas, waypoints, hero points, etc. */
			enablePath(ch.path);
		}
	};

	auto isExpansionRoot = [](const std::string& path) -> bool
	{
		static const char* roots[] = {
			"tw_guides.tw_mc", "tw_guides.tw_mc_hot", "tw_guides.tw_mc_pof",
			"tw_guides.tw_mc_eod", "tw_guides.tw_mc_soto", "tw_guides.tw_mc_jw",
			"tw_guides.tw_mc_voe", "tw_guides.tw_mc_lws3",
			"tw_guides.tw_mc_lws4", "tw_guides.tw_mc_lws5",
		};
		for (const char* r : roots)
			if (ToLower(path) == r)
				return true;
		return false;
	};

	std::function<void(const std::vector<Category>&)> walk = [&](const std::vector<Category>& nodes)
	{
		for (const Category& n : nodes)
		{
			if (isExpansionRoot(n.path))
				visitExpansion(n);
			if (!n.children.empty())
				walk(n.children);
		}
	};

	if (!gMenu.empty())
	{
		walk(gMenu);
	}
	else
	{
		/* Pack menu not indexed yet — enable known Barefoot / Griffon folders only. */
		static const char* bareRoutes[] = {
			"tw_guides.tw_mc.tw_mc_trails2",
			"tw_guides.tw_mc_hot.tw_mc_hot_trails2",
			"tw_guides.tw_mc_pof.tw_mc_pof_trails2",
			"tw_guides.tw_mc_eod.tw_mc_eod_trails2",
			"tw_guides.tw_mc_lws3.tw_mc_lws3_trails2",
			"tw_guides.tw_mc_lws4.tw_mc_lws4_trails2",
			"tw_guides.tw_mc_lws5.tw_mc_lws5_trails2",
		};
		static const char* griffRoutes[] = {
			"tw_guides.tw_mc.tw_mc_trails",
			"tw_guides.tw_mc_hot.tw_mc_hot_trails",
			"tw_guides.tw_mc_pof.tw_mc_pof_trails",
			"tw_guides.tw_mc_eod.tw_mc_eod_trails",
			"tw_guides.tw_mc_lws3.tw_mc_lws3_trails",
			"tw_guides.tw_mc_lws4.tw_mc_lws4_trails",
			"tw_guides.tw_mc_lws5.tw_mc_lws5_trails",
		};
		const char** list = (routes == MapCompletionRoutes::Barefoot) ? bareRoutes : griffRoutes;
		const size_t n = (routes == MapCompletionRoutes::Barefoot)
			? (sizeof(bareRoutes) / sizeof(bareRoutes[0]))
			: (sizeof(griffRoutes) / sizeof(griffRoutes[0]));
		for (size_t i = 0; i < n; ++i)
			enablePath(list[i]);
	}

	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

void TekkitTrails::ClearMapCompletionCategories()
{
	std::lock_guard<std::mutex> lock(gMutex);

	auto isMcPath = [](const std::string& p) -> bool
	{
		const std::string low = ToLower(p);
		if (low == "tw_guides.tw_mc")
			return true;
		if (low.size() >= 16 && low.compare(0, 16, "tw_guides.tw_mc.") == 0)
			return true;
		if (low.size() >= 16 && low.compare(0, 16, "tw_guides.tw_mc_") == 0)
			return true;
		return false;
	};

	gEnabledPaths.erase(
		std::remove_if(gEnabledPaths.begin(), gEnabledPaths.end(),
			[&](const std::string& p) { return isMcPath(p); }),
		gEnabledPaths.end());
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

TekkitTrails::MapCompletionRoutes TekkitTrails::ActiveMapCompletionRoutes()
{
	std::lock_guard<std::mutex> lock(gMutex);
	bool bare = false;
	bool griff = false;
	for (const std::string& p : gEnabledPaths)
	{
		const std::string low = ToLower(p);
		const size_t dot = low.find_last_of('.');
		const std::string leaf = (dot == std::string::npos) ? low : low.substr(dot + 1);
		auto ends = [](const std::string& s, const char* suf) -> bool
		{
			const size_t n = std::strlen(suf);
			return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
		};
		/* Known Barefoot / Griffon folders (not SotO lanterns / VoE skimmer). */
		if (ends(leaf, "trails2") &&
			low.find("tw_mc_soto") == std::string::npos &&
			low.find("tw_mc_voe") == std::string::npos)
			bare = true;
		if (ends(leaf, "trails") && !ends(leaf, "trails2") && !ends(leaf, "trails3") &&
			low.find("tw_mc_soto") == std::string::npos &&
			low.find("tw_mc_voe") == std::string::npos &&
			low.find("tw_mc_jw") == std::string::npos)
			griff = true;
	}
	if (bare && !griff)
		return MapCompletionRoutes::Barefoot;
	if (griff && !bare)
		return MapCompletionRoutes::Griffon;
	return MapCompletionRoutes::None;
}

void TekkitTrails::EnableAllTekkitCategories()
{
	std::lock_guard<std::mutex> lock(gMutex);
	gEnabledPaths.clear();
	gEnabledPaths.push_back("tw_guides");
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

void TekkitTrails::DisableAllCategories()
{
	std::lock_guard<std::mutex> lock(gMutex);
	gEnabledPaths.clear();
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

std::vector<std::string> TekkitTrails::EnabledPaths()
{
	std::lock_guard<std::mutex> lock(gMutex);
	return gEnabledPaths;
}

void TekkitTrails::SetEnabledPaths(const std::vector<std::string>& paths)
{
	std::lock_guard<std::mutex> lock(gMutex);
	gEnabledPaths = paths;
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

void TekkitTrails::SerializeEnabledPaths(char* out, size_t outLen)
{
	if (!out || outLen == 0)
		return;
	out[0] = 0;
	const std::vector<std::string> paths = EnabledPaths();
	size_t used = 0;
	for (size_t i = 0; i < paths.size(); ++i)
	{
		const std::string& p = paths[i];
		if (p.empty())
			continue;
		const size_t need = p.size() + (used ? 1u : 0u);
		if (used + need + 1 >= outLen)
			break;
		if (used)
			out[used++] = '|';
		std::memcpy(out + used, p.c_str(), p.size());
		used += p.size();
		out[used] = 0;
	}
}

void TekkitTrails::ParseEnabledPaths(const char* pipeList)
{
	std::vector<std::string> paths;
	if (pipeList && pipeList[0])
	{
		std::string cur;
		for (const char* p = pipeList; ; ++p)
		{
			const char c = *p;
			if (c == '|' || c == ',' || c == '\n' || c == '\r' || c == 0)
			{
				while (!cur.empty() && (cur.back() == ' ' || cur.back() == '\t'))
					cur.pop_back();
				size_t start = 0;
				while (start < cur.size() && (cur[start] == ' ' || cur[start] == '\t'))
					++start;
				if (start < cur.size())
					paths.push_back(cur.substr(start));
				cur.clear();
				if (c == 0)
					break;
				continue;
			}
			cur.push_back(c);
		}
	}
	SetEnabledPaths(paths);
}

bool TekkitTrails::OpenPathingFolder()
{
	const std::string hint = PathingFolderHint();
	if (hint.empty())
		return false;
	/* Ensure folder exists so Explorer has somewhere to land. */
	wchar_t wpath[MAX_PATH]{};
	if (MultiByteToWideChar(CP_UTF8, 0, hint.c_str(), -1, wpath, MAX_PATH) <= 0)
		return false;
	CreateDirectoryW(wpath, nullptr);
	const HINSTANCE r = ShellExecuteW(nullptr, L"explore", wpath, nullptr, nullptr, SW_SHOWNORMAL);
	return reinterpret_cast<INT_PTR>(r) > 32;
}

void TekkitTrails::SetSearchDestination(float continentX, float continentY)
{
	std::lock_guard<std::mutex> lock(gMutex);
	gGuideActive = true;
	gGuideDestX = continentX;
	gGuideDestY = continentY;
	RebuildSearchGuideLocked();
}

void TekkitTrails::ClearSearchGuide()
{
	std::lock_guard<std::mutex> lock(gMutex);
	gGuideActive = false;
	gGuide = {};
}

bool TekkitTrails::HasSearchGuide()
{
	std::lock_guard<std::mutex> lock(gMutex);
	return gGuideActive && gGuide.points.size() >= 2;
}

TekkitTrails::Trail TekkitTrails::SearchGuide()
{
	std::lock_guard<std::mutex> lock(gMutex);
	Trail slim{};
	slim.mapId = gGuide.mapId;
	slim.color = gGuide.color;
	std::snprintf(slim.label, sizeof(slim.label), "%s", gGuide.label);
	slim.points = gGuide.points; /* continent only for minimap */
	return slim;
}

bool TekkitTrails::DrawSettings()
{
	bool dirty = false;

	/* Keep packs indexing while the panel is open (even if overlays are off). */
	uint32_t mapId = 0;
	if (G::Mumble && G::Mumble->uiTick != 0)
	{
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		if (ctx)
			mapId = ctx->mapId;
	}
	Update(mapId ? mapId : 1u);

	dirty |= ImGui::Checkbox("Enable path overlays", &G::ShowTekkitTrails);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Master switch — loads packs and allows compass / world drawing.");
	if (!G::ShowTekkitTrails)
		return dirty;

	dirty |= ImGui::Checkbox("Draw on in-game compass", &G::ShowCompassOverlay);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("TacO / Blish style — project enabled markers onto the stock compass.");
	dirty |= ImGui::Checkbox("In-world GPS trails", &G::ShowWorldTrails);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("3D world breadcrumbs near you (same categories as the compass).");
	if (G::ShowWorldTrails)
	{
		dirty |= ImGui::SliderFloat("GPS range (m)", &G::WorldTrailMaxDist, 40.f, 200.f, "%.0f");
		dirty |= ImGui::SliderFloat("GPS width (× Blish)", &G::WorldTrailWidth, 0.5f, 4.0f, "%.1f");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Multiplier on Blish/TacO world trail width (~20\" × pack trailscale).\n"
				"1.0 matches Blish default; raise if you want thicker ribbons.");
	}
	dirty |= ImGui::Checkbox("Hide when world map open", &G::HideWhenMapOpen);
	dirty |= ImGui::Checkbox("Hide out of gameplay", &G::HideOutOfGameplay);

	ImGui::Separator();
	ImGui::TextUnformatted("Packs");
	const std::string pathHint = PathingFolderHint();
	ImGui::TextDisabled("%s", pathHint.c_str());
	if (ImGui::Button("Reload packs"))
		ReloadPacks();
	ImGui::SameLine();
	if (ImGui::Button("Open folder"))
		OpenPathingFolder();
	if (IsLoading())
	{
		ImGui::SameLine();
		ImGui::TextDisabled("Loading…");
	}

	const bool loading = IsLoading();
	const std::vector<std::string> packs = LoadedPackNames();
	if (loading)
		ImGui::TextDisabled("Packs: %d  ·  indexing categories…", PackCount());
	else
		ImGui::TextDisabled("Packs: %d  ·  This map: %d trails, %d markers on",
			PackCount(), TrailCount(), MarkerCount());
	if (!packs.empty())
	{
		ImGui::BeginChild("##igh_tekkit_packs", ImVec2(0.f, 48.f), true);
		for (const std::string& name : packs)
			ImGui::BulletText("%s", name.c_str());
		ImGui::EndChild();
	}
	if (PackCount() == 0 && !loading)
	{
		ImGui::TextColored(ImVec4(1.f, 0.7f, 0.3f, 1.f),
			"No .taco packs — drop Tekkit's All-In-One into the pathing folder.");
		return dirty;
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Categories");
	ImGui::TextDisabled("Like Blish / TacO / Taimi: parent on = all children. Saved between sessions.");

	ImGui::TextDisabled("Pick one route edition — never both.");
	const MapCompletionRoutes activeMc = ActiveMapCompletionRoutes();
	bool bareOn = (activeMc == MapCompletionRoutes::Barefoot);
	bool griffOn = (activeMc == MapCompletionRoutes::Griffon);
	if (ImGui::Checkbox("Map Completion - Foot###gw2igh_tekkit_mc_bare", &bareOn))
	{
		if (bareOn)
			EnableMapCompletionPreset(MapCompletionRoutes::Barefoot);
		else
			ClearMapCompletionCategories();
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Hearts / POIs / vistas + Barefoot routes only.\n"
			"Turns Griffon routes off.");
	ImGui::SameLine();
	if (ImGui::Checkbox("Map Completion - Griffon###gw2igh_tekkit_mc_griff", &griffOn))
	{
		if (griffOn)
			EnableMapCompletionPreset(MapCompletionRoutes::Griffon);
		else
			ClearMapCompletionCategories();
		dirty = true;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Hearts / POIs / vistas + Griffon routes only.\n"
			"Turns Barefoot routes off.");
	ImGui::SameLine();
	if (ImGui::Button("All Tekkit"))
	{
		EnableAllTekkitCategories();
		dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("All off"))
	{
		DisableAllCategories();
		dirty = true;
	}

	static char sFilter[96]{};
	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputTextWithHint("###gw2igh_tekkit_filter", "Filter categories...", sFilter, sizeof(sFilter));
	/* Keep keys on this field even if the pointer drifts off the pad. */
	if (ImGui::IsItemActive())
	{
		ImGui::GetIO().WantCaptureKeyboard = true;
		ImGui::CaptureKeyboardFromApp(true);
	}

	static uint64_t sTreeRevision = 0;
	static std::vector<Category> tree;
	const uint64_t revision = gMenuRevision.load(std::memory_order_acquire);
	if (revision != sTreeRevision)
	{
		/* Don't wipe the tree on a transient lock miss (Blish never blanks the menu). */
		std::vector<Category> next = CategoryTree();
		if (!next.empty())
		{
			tree = std::move(next);
			sTreeRevision = revision;
		}
		else if (!IsLoading())
		{
			tree.clear();
			sTreeRevision = revision;
		}
	}
	if (tree.empty())
	{
		ImGui::TextDisabled(loading ? "Indexing Tekkit menu…" : "No categories yet — wait for pack index.");
		return dirty;
	}

	/* O(N) visibility mask — per-node recursive search was O(N^2) and froze
	   typing on Tekkit's large category tree. */
	static char sFilterBuilt[96]{};
	static uint64_t sFilterTreeRev = 0;
	static std::unordered_set<std::string> sFilterShow;
	const bool filterOn = sFilter[0] != 0;
	if (!filterOn)
	{
		sFilterBuilt[0] = 0;
		sFilterShow.clear();
		sFilterTreeRev = 0;
	}
	else if (std::strcmp(sFilter, sFilterBuilt) != 0 || sFilterTreeRev != sTreeRevision)
	{
		std::memcpy(sFilterBuilt, sFilter, sizeof(sFilterBuilt));
		sFilterTreeRev = sTreeRevision;
		sFilterShow.clear();

		auto toLower = [](std::string s) -> std::string
		{
			for (char& ch : s)
				if (ch >= 'A' && ch <= 'Z')
					ch = static_cast<char>(ch - 'A' + 'a');
			return s;
		};
		const std::string needle = toLower(sFilter);

		std::function<bool(const Category&)> mark = [&](const Category& node) -> bool
		{
			if (node.hidden)
				return false;
			bool hit = toLower(node.label).find(needle) != std::string::npos ||
				toLower(node.path).find(needle) != std::string::npos;
			for (const Category& ch : node.children)
				hit = mark(ch) || hit;
			if (hit)
				sFilterShow.insert(node.path);
			return hit;
		};
		for (const Category& c : tree)
			mark(c);
	}

	const float listH = std::max(180.f, ImGui::GetContentRegionAvail().y - 28.f);
	ImGui::BeginChild("##tekkit_cats", ImVec2(0.f, listH), true);
	std::function<void(Category&)> draw = [&](Category& c)
	{
		if (c.hidden)
			return;
		if (filterOn && sFilterShow.find(c.path) == sFilterShow.end())
			return;

		ImGui::PushID(c.path.c_str());
		if (c.separator)
		{
			ImGui::Spacing();
			ImGui::TextDisabled("%s", c.label.c_str());
			for (Category& ch : c.children)
				draw(ch);
			ImGui::PopID();
			return;
		}

		const bool hasKids = !c.children.empty();
		bool en = c.enabled;

		if (hasKids)
		{
			if (ImGui::Checkbox("##en", &en))
			{
				SetCategoryEnabled(c.path, en);
				dirty = true;
			}
			ImGui::SameLine();
			char tip[192];
			if (c.trails > 0)
				std::snprintf(tip, sizeof(tip), "%s  (%d)", c.label.c_str(), c.trails);
			else
				std::snprintf(tip, sizeof(tip), "%s", c.label.c_str());
			if (filterOn)
				ImGui::SetNextItemOpen(true); /* expand matches while filtering */
			const bool open = ImGui::TreeNodeEx(tip,
				ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", c.path.c_str());
			if (open)
			{
				for (Category& ch : c.children)
					draw(ch);
				ImGui::TreePop();
			}
		}
		else
		{
			char label[192];
			if (c.trails > 0)
				std::snprintf(label, sizeof(label), "%s  (%d)", c.label.c_str(), c.trails);
			else
				std::snprintf(label, sizeof(label), "%s", c.label.c_str());
			if (ImGui::Checkbox(label, &en))
			{
				SetCategoryEnabled(c.path, en);
				dirty = true;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", c.path.c_str());
		}
		ImGui::PopID();
	};
	for (Category& c : tree)
		draw(c);
	ImGui::EndChild();

	if (!loading && TrailCount() == 0 && MarkerCount() == 0)
		ImGui::TextColored(ImVec4(1.f, 0.75f, 0.35f, 1.f),
			"Nothing visible on this map — enable categories above.");
	else
		ImGui::TextDisabled("Checked categories draw on compass + world GPS.");

	return dirty;
}
