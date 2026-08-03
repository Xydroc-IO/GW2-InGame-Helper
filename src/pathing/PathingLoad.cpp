#include "PathingTrails.h"

#include "Globals.h"
#include "PathingIndex.h"
#include "PathingParse.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>
#include <winhttp.h>
#include "miniz/miniz.h"

namespace PathingDetail
{

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

	bool TypeHasLadyMountShortcut(const std::string& typeLow);
	bool IsLadyShortcutTypeLabel(const char* label);

	bool IsMountShortcutMarker(const PathingTrails::Marker& marker)
	{
		/* Type path first — iconId is empty until pack extract + Nexus upload. */
		if (IsLadyShortcutTypeLabel(marker.label))
			return true;
		if (!marker.iconId[0])
			return false;
		/* IconTextureId keeps alnum from path → …Images_Mounts_Mount_Raptor… */
		return std::strstr(marker.iconId, "Mounts") != nullptr ||
			std::strstr(marker.iconId, "mounts") != nullptr;
	}

	bool MarkerShownInWorld(const PathingTrails::Marker& marker)
	{
		if (IsMountShortcutMarker(marker))
			return true;
		return marker.inGameVisible;
	}

	bool MarkerShownOnCompass(const PathingTrails::Marker& marker)
	{
		if (IsMountShortcutMarker(marker))
			return true;
		return marker.minimapVisible;
	}










	/* Merge a parsed category subtree into dest by path (official overlay depth). */

	/* (Prune removed — keep every Tekkit MarkerCategory toggle.) */



	/* Lady map-completion editions live at legs.map.<region>.<map>.<edition>…
	   Editions: barefoot | all/main/withmounts | wp. Do not treat "main"/"wp"
	   outside legs.map (festivals/chests) as a route edition. */
	bool LadyMapRouteEdition(const std::string& typeLow, std::string& outEdition)
	{
		if (typeLow.size() < 10)
			return false;
		if (typeLow.compare(0, 9, "legs.map.") != 0 &&
			typeLow.compare(0, 9, "leag.map.") != 0)
			return false;
		size_t start = 9; /* after legs.map. / leag.map. */
		for (int i = 0; i < 2; ++i)
		{
			const size_t dot = typeLow.find('.', start);
			if (dot == std::string::npos)
				return false;
			start = dot + 1;
		}
		const size_t dot = typeLow.find('.', start);
		outEdition = (dot == std::string::npos)
			? typeLow.substr(start)
			: typeLow.substr(start, dot - start);
		return !outEdition.empty();
	}

	bool IsLadyWithMountsEdition(const std::string& seg)
	{
		return seg == "all" || seg == "main" || seg == "withmounts";
	}

	bool IsLadyRouteEditionSeg(const std::string& seg)
	{
		return seg == "barefoot" || IsLadyWithMountsEdition(seg) || seg == "wp";
	}

	/* Mount shortcut category leaves (icons under Data/Images/Mounts/). */
	bool IsLadyMountShortcutSeg(const std::string& seg)
	{
		return seg == "mount" || seg == "mounted" || seg == "mounts" ||
			seg == "beetle" || seg == "griffon" || seg == "jackal" ||
			seg == "raptor" || seg == "skimmer" || seg == "skyscale" ||
			seg == "skyscal" || seg == "springer" || seg == "warclaw" ||
			seg == "turtle" || seg == "dismount" || seg == "leap" ||
			seg == "hover" || seg == "bof";
	}

	bool TypeHasLadyMountShortcut(const std::string& typeLow)
	{
		size_t start = 0;
		while (start <= typeLow.size())
		{
			const size_t dot = typeLow.find('.', start);
			const std::string seg = (dot == std::string::npos)
				? typeLow.substr(start)
				: typeLow.substr(start, dot - start);
			if (IsLadyMountShortcutSeg(seg))
				return true;
			if (dot == std::string::npos)
				break;
			start = dot + 1;
		}
		return false;
	}

	bool IsLadyShortcutTypeLabel(const char* label)
	{
		if (!label || !label[0])
			return false;
		const std::string typeLow = ToLower(label);
		if (typeLow.find(".bfs.") != std::string::npos)
			return true;
		return TypeHasLadyMountShortcut(typeLow);
	}

	/* Calibrated so GPS width 1.0 matches “correct” size for each edition
	   (user: Mounts ~2.0, Barefoot/WP ~4.0 on the old unnormalized slider). */
	float LadyGpsWidthBias(const char* label)
	{
		if (!label || !label[0])
			return 1.f;
		std::string ed;
		if (!LadyMapRouteEdition(ToLower(label), ed))
			return 1.f;
		if (ed == "barefoot" || ed == "wp")
			return 4.f;
		if (IsLadyWithMountsEdition(ed))
			return 2.f;
		return 1.f;
	}

	bool TypeCategoryEnabled(const std::string& type, const std::vector<std::string>& enabled)
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

	bool TypeEnabledWithEnabled(const std::string& type, const std::vector<std::string>& enabled)
	{
		if (type.empty() || enabled.empty())
			return false;
		const std::string typeLow = ToLower(type);

		/* Lady Elyssa map-completion Features: Barefoot | With Mounts | WP Only
		   are mutually exclusive route editions (same idea as Tekkit Foot/Griffon). */
		const bool ladyPack =
			typeLow == "legs" || typeLow == "leag" ||
			(typeLow.size() > 5 && typeLow.compare(0, 5, "legs.") == 0) ||
			(typeLow.size() > 5 && typeLow.compare(0, 5, "leag.") == 0);
		if (ladyPack)
		{
			const bool bareOn = G::LadyBarefoot;
			const bool wpOn = G::LadyWpOnly;
			const bool mountsOn = G::LadyWithMounts;
			const bool anyEdition = bareOn || wpOn || mountsOn;
			const bool barefootShortcut = typeLow.find(".bfs.") != std::string::npos;
			std::string mapEd;
			if (LadyMapRouteEdition(typeLow, mapEd))
			{
				/* No Features edition selected → hide all legs.map route content. */
				if (!anyEdition)
					return false;

				/* Barefoot Shortcuts live at legs.map.*.bfs.<mount> — Barefoot only. */
				if (barefootShortcut ||
					(IsLadyMountShortcutSeg(mapEd) && !IsLadyRouteEditionSeg(mapEd)))
				{
					if (!bareOn)
						return false;
				}
				else if (mapEd == "wp")
				{
					if (!wpOn)
						return false;
				}
				else if (mapEd == "barefoot")
				{
					if (!bareOn)
						return false;
				}
				else if (IsLadyWithMountsEdition(mapEd))
				{
					if (!mountsOn)
					{
						/* Maps with no barefoot edition: Barefoot still needs a route. */
						if (!(bareOn && gMapsWithLadyBarefoot.count(gActiveMap) == 0))
							return false;
					}
				}
				else if (IsLadyRouteEditionSeg(mapEd))
					return false;
			}
			else if (TypeHasLadyMountShortcut(typeLow))
			{
				/* Non-map mount POIs (adventures/chests/…) follow With Mounts. */
				if (barefootShortcut)
				{
					if (!bareOn)
						return false;
				}
				else if (!mountsOn)
					return false;
			}
		}

		return TypeCategoryEnabled(type, enabled);
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
			/* Ancestor covers this node, or this node covers an enabled child. */
			if (PrefixMatchesType(low, el) || PrefixMatchesType(el, low) || low == el)
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

	void RebuildSearchGuideLocked()
	{
		if (!gGuideActive || gCurrentAll.empty())
		{
			/* Keep any previous guide polyline — clearing here makes the orange
			   ribbon blink whenever packs are mid-reload. */
			return;
		}

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
			const PathingTrails::Trail& tr = gCurrentAll[t];
			if (tr.points.size() < 2)
				continue;

			int di = 0;
			int pi = 0;
			float bestD = 1e30f;
			float bestP = 1e30f;
			for (size_t i = 0; i < tr.points.size(); ++i)
			{
				if (!std::isfinite(tr.points[i].x) || !std::isfinite(tr.points[i].y))
					continue;
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

			/* Prefer trails that still have world samples — in-world GPS needs them. */
			float score = havePlayer ? (bestD + bestP * 0.85f) : bestD;
			if (tr.worldPoints.size() == tr.points.size() && tr.worldPoints.size() >= 2)
				score *= 0.55f;
			if (score < bestScore)
			{
				bestScore = score;
				bestTrail = static_cast<int>(t);
				bestDi = di;
				bestPi = havePlayer ? pi : 0;
			}
		}

		if (bestTrail < 0)
			return; /* caller may force-reload; keep prior until a better snap exists */

		const PathingTrails::Trail& src = gCurrentAll[static_cast<size_t>(bestTrail)];
		int a = bestPi;
		int b = bestDi;
		if (a > b)
			std::swap(a, b);
		a = std::max(0, a - 1);
		b = std::min(static_cast<int>(src.points.size()) - 1, b + 1);
		if (b - a < 1)
			return;

		PathingTrails::Trail next{};
		next.mapId = src.mapId;
		next.color = 0xFFFFAA20u;
		std::snprintf(next.label, sizeof(next.label), "Search route · %s", src.label);
		next.points.assign(src.points.begin() + a, src.points.begin() + b + 1);
		if (src.worldPoints.size() == src.points.size())
		{
			next.worldPoints.assign(
				src.worldPoints.begin() + a, src.worldPoints.begin() + b + 1);
		}
		if (bestPi > bestDi)
		{
			std::reverse(next.points.begin(), next.points.end());
			std::reverse(next.worldPoints.begin(), next.worldPoints.end());
		}
		gGuide = std::move(next);
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

	void LoadMapTrails(uint32_t mapId, uint32_t epoch)
	{
		std::vector<IndexedTrail> indexCopy;
		std::vector<IndexedPoi> poiCopy;
		std::vector<std::string> enabledCopy;
		std::unordered_map<std::string, MarkerStyle> styleCopy;
		uint32_t enabledGen = 0;
		bool guideActive = false;
		{
			std::lock_guard<std::mutex> lock(gMutex);
			indexCopy = gIndex;
			poiCopy = gPoiIndex;
			enabledCopy = gEnabledPaths;
			styleCopy = gCategoryStyles;
			enabledGen = gEnabledGen.load(std::memory_order_acquire);
			guideActive = gGuideActive;
		}

		/* Nothing opted in and no active search → skip opening the ~100MB pack.
		   This was a common Wine OOM path when every category defaulted on. */
		const bool needPack = !enabledCopy.empty() || guideActive;
		if (!needPack)
		{
			std::lock_guard<std::mutex> lock(gMutex);
			if (gEpoch.load(std::memory_order_acquire) != epoch)
				return;
			gActiveMap = mapId;
			gLoadedEnabledGen = enabledGen;
			gCurrentAll.clear();
			gCurrentMarkers.clear();
			gMapsWithLadyBarefoot.erase(mapId);
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
		bool mapHasBarefoot = false;
		for (const IndexedTrail& it : indexCopy)
		{
			if (it.mapId != mapId)
				continue; /* header-resolved; unknown (0) trails are skipped */
			int rank;
			/* Category enable only — Lady Barefoot/WP/Mounts filter at draw time. */
			if (TypeCategoryEnabled(it.type, enabledCopy))
				rank = 0;
			else if (it.mapCompletion)
				rank = 1;
			else
				rank = 2;
			std::string ed;
			if (rank == 0 && LadyMapRouteEdition(ToLower(it.type), ed) && ed == "barefoot")
				mapHasBarefoot = true;
			cands.push_back({&it, rank});
		}

		/* Prefer Lady barefoot / wp / mounts editions so a trail cap cannot drop
		   the routes Features toggles need. Then pack path, then rank. */
		auto ladyEdPrio = [](const std::string& type) -> int
		{
			std::string ed;
			if (!LadyMapRouteEdition(ToLower(type), ed))
				return 50;
			if (ed == "barefoot")
				return 0;
			if (ed == "wp")
				return 1;
			if (IsLadyWithMountsEdition(ed))
				return 2;
			return 40;
		};
		std::stable_sort(cands.begin(), cands.end(),
			[&](const Cand& a, const Cand& b) {
				if (a.rank != b.rank)
					return a.rank < b.rank;
				const int pa = (a.rank == 0) ? ladyEdPrio(a.it->type) : 50;
				const int pb = (b.rank == 0) ? ladyEdPrio(b.it->type) : 50;
				if (pa != pb)
					return pa < pb;
				return a.it->packPath < b.it->packPath;
			});

		std::vector<PathingTrails::Trail> loaded;
		loaded.reserve(64);
		std::unordered_map<std::string, std::wstring> assetsNeeded;
		std::unordered_set<std::string> seenTrailFiles;
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
			/* Same .trl from two packs (duplicate Tekkit AIO copies) → skip. */
			const std::string trailKey = ToLower(it.entryName) + "#" +
				std::to_string(static_cast<unsigned>(mapId));
			if (!seenTrailFiles.insert(trailKey).second)
				continue;
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
			std::vector<PathingTrails::WorldPoint> world;
			if (!ParseTrl(bytes, trailMap, world) || trailMap != mapId)
				continue;

			PathingTrails::Trail trail{};
			const MarkerStyle style = ResolveStyle(it.type, it.style, styleCopy);
			trail.mapId = mapId;
			trail.color = style.hasColor ? style.color : it.color;
			trail.minimapVisible = style.minimapVisible;
			trail.inGameVisible = style.inGameVisible;
			/* Blish: mapVisibility is the fullscreen map, not in-world GPS. */
			{
				const std::string typeLow = ToLower(it.type);
				if (typeLow.compare(0, 9, "legs.map.") == 0 ||
					typeLow.compare(0, 9, "leag.map.") == 0)
				{
					/* Lady map routes: always drawable on our compass + world GPS. */
					if (!style.hasInGameVisible)
						trail.inGameVisible = true;
					trail.minimapVisible = true;
				}
				else if (style.hasMapVisible && !style.hasInGameVisible)
					trail.inGameVisible = style.mapVisible;
			}
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
			trail.worldPoints.reserve(world.size());
			for (const PathingTrails::WorldPoint& w : world)
			{
				if (!std::isfinite(w.x) || !std::isfinite(w.y) || !std::isfinite(w.z))
				{
					/* Preserve TacO section break so compass/world don't stitch segments. */
					if (!trail.points.empty() && std::isfinite(trail.points.back().x))
					{
						trail.points.push_back({NAN, NAN});
						trail.worldPoints.push_back({NAN, NAN, NAN});
					}
					continue;
				}
				PathingTrails::Point cc{};
				WorldToContinent(rects, w.x, w.z, cc.x, cc.y);
				if (!std::isfinite(cc.x) || !std::isfinite(cc.y))
					continue;
				trail.points.push_back(cc);
				trail.worldPoints.push_back(w);
			}
			if (trail.points.size() < 2)
				continue;
			if (c.rank != 0)
				++otherCount;
			loaded.push_back(std::move(trail));
		}

		/* POI markers for this map — same TacO prefix enable rules as trails.
		   Prefer Barefoot Shortcuts / Mounts icons so the per-map cap cannot
		   drop them behind heart/festival POI spam. */
		std::vector<const IndexedPoi*> poiCands;
		poiCands.reserve(512);
		for (const IndexedPoi& poi : poiCopy)
		{
			if (poi.mapId != mapId)
				continue;
			if (!TypeCategoryEnabled(poi.type, enabledCopy))
				continue;
			poiCands.push_back(&poi);
		}
		std::stable_sort(poiCands.begin(), poiCands.end(),
			[](const IndexedPoi* a, const IndexedPoi* b) {
				auto prio = [](const IndexedPoi& p) -> int {
					const std::string low = ToLower(p.type);
					if (low.find(".bfs.") != std::string::npos)
						return 0;
					if (TypeHasLadyMountShortcut(low))
						return 1;
					return 2;
				};
				return prio(*a) < prio(*b);
			});

		std::vector<PathingTrails::Marker> markers;
		markers.reserve(std::min(poiCands.size(), kMaxMarkersPerMap));
		for (const IndexedPoi* poiPtr : poiCands)
		{
			const IndexedPoi& poi = *poiPtr;
			const MarkerStyle style = ResolveStyle(poi.type, poi.style, styleCopy);
			PathingTrails::Point cc{};
			WorldToContinent(rects, poi.wx, poi.wz, cc.x, cc.y);
			if (!std::isfinite(cc.x) || !std::isfinite(cc.y))
				continue;
			PathingTrails::Marker m{};
			m.mapId = mapId;
			m.color = style.hasColor ? style.color : 0xFFFFCC33u;
			m.pos = cc;
			m.world = {poi.wx, poi.wy, poi.wz};
			m.minimapVisible = style.minimapVisible;
			m.inGameVisible = style.inGameVisible;
			/* Blish separates mapVisibility (fullscreen map) from inGameVisibility.
			   Do NOT copy mapVisibility=0 onto in-world — Lady Mounts categories set
			   mapVisibility/miniMapVisibility to 0 but still draw in the world. */
			{
				const std::string typeLow = ToLower(poi.type);
				if (typeLow.compare(0, 9, "legs.map.") == 0 ||
					typeLow.compare(0, 9, "leag.map.") == 0)
				{
					if (m.inGameVisible)
						m.minimapVisible = true;
				}
			}
			m.mapDisplaySize = std::max(1.f, style.mapDisplaySize);
			m.minSize = std::max(1.f, style.minSize);
			m.maxSize = std::max(m.minSize, style.maxSize);
			m.iconSize = std::max(0.05f, style.iconSize);
			m.heightOffset = style.heightOffset;
			m.fadeNear = style.fadeNear;
			m.fadeFar = style.fadeFar;
			m.alpha = std::clamp(style.alpha, 0.f, 1.f);
			std::snprintf(m.label, sizeof(m.label), "%s", poi.type.c_str());
			if (!poi.guid.empty())
				std::snprintf(m.guid, sizeof(m.guid), "%s", poi.guid.c_str());
			m.behavior = style.behavior;
			m.autoTrigger = style.autoTrigger;
			m.triggerRange = style.hasTriggerRange ? style.triggerRange : 2.f;
			m.resetLength = style.resetLength;
			m.invertBehavior = style.invertBehavior;
			if (style.hasHide)
				std::snprintf(m.hide, sizeof(m.hide), "%s", style.hide.c_str());
			if (style.hasShow)
				std::snprintf(m.show, sizeof(m.show), "%s", style.show.c_str());
			if (style.hasTipName)
				std::snprintf(m.tipName, sizeof(m.tipName), "%s", style.tipName.c_str());
			if (style.hasTipDescription)
				std::snprintf(m.tipDescription, sizeof(m.tipDescription), "%s",
					style.tipDescription.c_str());
			if (style.hasInfo)
				std::snprintf(m.info, sizeof(m.info), "%s", style.info.c_str());
			if (style.hasCopy)
				std::snprintf(m.copy, sizeof(m.copy), "%s", style.copy.c_str());
			if (style.hasCopyMessage)
				std::snprintf(m.copyMessage, sizeof(m.copyMessage), "%s",
					style.copyMessage.c_str());
			const std::string& icon = style.iconFile;
			if (!icon.empty())
			{
				const std::string tid = IconTextureId(icon);
				std::snprintf(m.iconId, sizeof(m.iconId), "%s", tid.c_str());
				assetsNeeded.emplace(icon, poi.packPath);
				const std::string iconLow = ToLower(icon);
				/* Same rules for Numbers and Mounts PNGs — Lady often sets
				   mapVisibility=0; that is the fullscreen map, not in-world/compass. */
				if (iconLow.find("images/mounts/") != std::string::npos ||
					iconLow.find("images/numbers/") != std::string::npos)
				{
					m.minimapVisible = true;
					m.inGameVisible = true;
					m.mapDisplaySize = std::max(m.mapDisplaySize, 24.f);
					m.minSize = std::max(m.minSize, 16.f);
					m.maxSize = std::max(m.maxSize, 48.f);
					m.iconSize = std::max(m.iconSize, 1.f);
					if (!m.tipName[0] &&
						iconLow.find("images/mounts/") != std::string::npos)
					{
						std::string leaf = icon;
						const size_t slash = leaf.find_last_of("/\\");
						if (slash != std::string::npos)
							leaf = leaf.substr(slash + 1);
						const size_t dot = leaf.find_last_of('.');
						if (dot != std::string::npos)
							leaf = leaf.substr(0, dot);
						if (leaf.rfind("Mount_", 0) == 0)
							leaf = leaf.substr(6);
						for (char& ch : leaf)
						{
							if (ch == '_' || ch == '-')
								ch = ' ';
						}
						std::snprintf(m.tipName, sizeof(m.tipName), "%s", leaf.c_str());
					}
				}
			}
			markers.push_back(std::move(m));
			if (markers.size() >= kMaxMarkersPerMap)
				break;
		}

		/* Free trail zip before icon pass — Wine cannot hold two 45MB packs. */
		pack.Close();
		openPath.clear();

		/* Extract icons — trail chevrons first (missing → solid line), then Mounts,
		   then Numbers. Never rescan the whole 45MB zip (Wine OOM). */
		if (!assetsNeeded.empty())
		{
			static const char* kLadyMountIcons[] = {
				"Data/Images/Mounts/Mount_Raptor.png",
				"Data/Images/Mounts/Mount_Springer.png",
				"Data/Images/Mounts/Mount_Skimmer.png",
				"Data/Images/Mounts/Mount_Jackal.png",
				"Data/Images/Mounts/Mount_Griffon.png",
				"Data/Images/Mounts/Mount_Beetle.png",
				"Data/Images/Mounts/Mount_Warclaw.png",
				"Data/Images/Mounts/Mount_Skyscale.png",
				"Data/Images/Mounts/Dismount.png",
				"Data/Images/Mounts/Leap.png",
				"Data/Images/Mounts/Hover.png",
				nullptr
			};
			/* Lady map-completion inherits these — guarantee they upload. */
			static const char* kLadyTrailIcons[] = {
				"Data/Images/Trails/White Arrow Black Border.png",
				"Data/Images/Trails/Footprints.png",
				"Data/Images/Trails/Trail Pointer - Small.png",
				"Data/Images/Trails/Dashed Lines - Fine with Shadow.png",
				"Data/Images/Trail - Stubby.png",
				"Data/Images/trailarrow.png",
				"Data/Images/trailarrow-small.png",
				"Data/Images/trailarrow-two-tone.png",
				"Data/Images/Line - Heart.png",
				nullptr
			};
			std::wstring mountPack;
			for (const Cand& c : cands)
			{
				if (c.rank == 0 && c.it && !c.it->packPath.empty())
				{
					mountPack = c.it->packPath;
					break;
				}
			}
			for (const IndexedPoi* poiPtr : poiCands)
			{
				if (poiPtr && !poiPtr->packPath.empty())
				{
					if (mountPack.empty())
						mountPack = poiPtr->packPath;
					break;
				}
			}
			if (mountPack.empty() && !assetsNeeded.empty())
				mountPack = assetsNeeded.begin()->second;
			if (!mountPack.empty())
			{
				for (int i = 0; kLadyTrailIcons[i]; ++i)
					assetsNeeded.emplace(kLadyTrailIcons[i], mountPack);
				for (int i = 0; kLadyMountIcons[i]; ++i)
					assetsNeeded.emplace(kLadyMountIcons[i], mountPack);
			}

			std::vector<std::pair<std::string, std::wstring>> iconList(
				assetsNeeded.begin(), assetsNeeded.end());
			std::sort(iconList.begin(), iconList.end(),
				[](const auto& a, const auto& b) {
					auto prio = [](const std::string& path) {
						const std::string low = ToLower(path);
						/* Paths first — cyan/white line means these never uploaded. */
						if (low.find("images/trails/") != std::string::npos ||
							low.find("/trails/") != std::string::npos ||
							low.find("trailarrow") != std::string::npos ||
							low.find("trail -") != std::string::npos ||
							low.find("trail_") != std::string::npos ||
							low.find("footprints") != std::string::npos)
							return 0;
						if (low.find("images/mounts/") != std::string::npos)
							return 1;
						if (low.find("images/numbers/") != std::string::npos)
							return 2;
						return 3;
					};
					const int pa = prio(a.first);
					const int pb = prio(b.first);
					if (pa != pb)
						return pa < pb;
					return a.second < b.second;
				});
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
				DecodeXmlEntities(entry);
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
				if (idx < 0 && entry.rfind("POIs/", 0) == 0)
					idx = ZipLocate(iconPack.zip, entry.substr(5));
				if (idx < 0)
					idx = ZipLocate(iconPack.zip, std::string("POIs/") + entry);
				if (idx < 0 && entry.rfind("Data/", 0) == 0)
					idx = ZipLocate(iconPack.zip, std::string("POIs/") + entry);
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
		if (mapHasBarefoot)
			gMapsWithLadyBarefoot.insert(mapId);
		else
			gMapsWithLadyBarefoot.erase(mapId);
		gContentRevision.fetch_add(1, std::memory_order_release);
		RebuildSearchGuideLocked();
	}


	void InsertCatPath(std::vector<PathingTrails::Category>& roots, const std::string& type)
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

		std::vector<PathingTrails::Category>* level = &roots;
		std::string path;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			if (!path.empty())
				path += '.';
			path += parts[i];
			PathingTrails::Category* found = nullptr;
			for (PathingTrails::Category& c : *level)
			{
				if (c.path == path)
				{
					found = &c;
					break;
				}
			}
			if (!found)
			{
				PathingTrails::Category neu;
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

	void MarkEnabled(std::vector<PathingTrails::Category>& nodes)
	{
		for (PathingTrails::Category& c : nodes)
		{
			c.enabled = CategoryUiEnabledLocked(c.path);
			MarkEnabled(c.children);
		}
	}

} // namespace PathingDetail
