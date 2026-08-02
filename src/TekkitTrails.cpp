#include "TekkitTrails.h"

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
	constexpr size_t kMaxZipBytes = 120u * 1024u * 1024u;
	constexpr size_t kMaxTrailFile = 8u * 1024u * 1024u;
	constexpr size_t kMaxPointsPerTrail = 512;
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

	struct MarkerStyle
	{
		std::string iconFile;
		bool hasIconFile = false;
		std::string texture;
		bool hasTexture = false;
		bool minimapVisible = true;
		bool hasMinimapVisible = false;
		bool mapVisible = true;
		bool hasMapVisible = false;
		bool inGameVisible = true;
		bool hasInGameVisible = false;
		float mapDisplaySize = 20.f;
		bool hasMapDisplaySize = false;
		float minSize = 5.f;
		bool hasMinSize = false;
		float maxSize = 2048.f;
		bool hasMaxSize = false;
		float iconSize = 1.f;
		bool hasIconSize = false;
		float heightOffset = 1.5f;
		bool hasHeightOffset = false;
		float fadeNear = -1.f;
		bool hasFadeNear = false;
		float fadeFar = -1.f;
		bool hasFadeFar = false;
		float alpha = 1.f;
		bool hasAlpha = false;
		float trailScale = 1.f;
		bool hasTrailScale = false;
		uint32_t color = 0xFFFFFFFFu;
		bool hasColor = false;
	};

	struct IndexedTrail
	{
		std::wstring packPath;
		std::string  entryName;
		std::string  type;
		uint32_t     color = 0xFFFFFFFFu;
		uint32_t     mapId = 0; /* from .trl header; 0 = unknown */
		int          fileIndex = -1; /* zip central-dir index within its pack */
		bool         mapCompletion = false;
		MarkerStyle  style;
	};

	struct IndexedPoi
	{
		std::wstring packPath;
		std::string  type;
		uint32_t     mapId = 0;
		float        wx = 0.f;
		float        wy = 0.f;
		float        wz = 0.f;
		MarkerStyle  style;
	};

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

	std::string ToLower(std::string s)
	{
		for (char& c : s)
			if (c >= 'A' && c <= 'Z')
				c = static_cast<char>(c - 'A' + 'a');
		return s;
	}

	bool LooksLikeMapCompletion(const std::string& type, const std::string& path)
	{
		const std::string t = ToLower(type);
		const std::string p = ToLower(path);
		if (t.rfind("legs.map.", 0) == 0 || t.find(".map.") != std::string::npos)
			return true;
		if (t.rfind("tt.mc.", 0) == 0 || t.find(".mc.") != std::string::npos)
			return true;
		/* Tekkit uses tw_guides.tw_mc.… (underscore, not .mc.) */
		if (t.find(".tw_mc.") != std::string::npos || t.find("tw_mc.") != std::string::npos)
			return true;
		if (t.find("mapcompletion") != std::string::npos)
			return true;
		if (p.find("map completion") != std::string::npos)
			return true;
		if (p.find("map_completion") != std::string::npos)
			return true;
		if (p.find("/tw_mc_") != std::string::npos || p.find("tw_mc_") != std::string::npos)
			return true;
		if (p.find("/tw/mc/") != std::string::npos)
			return true;
		if (p.find("barefoot") != std::string::npos && p.find("map") != std::string::npos)
			return true;
		return false;
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

	uint32_t ParseColorAttr(const std::string& tag)
	{
		size_t p = tag.find("color=\"");
		if (p == std::string::npos)
			p = tag.find("Color=\"");
		if (p == std::string::npos)
			return 0xFF00FFFF;
		p = tag.find('"', p) + 1;
		size_t e = tag.find('"', p);
		if (e == std::string::npos || e <= p)
			return 0xFF00FFFF;
		std::string hex = tag.substr(p, e - p);
		if (!hex.empty() && hex[0] == '#')
			hex.erase(0, 1);
		if (hex.size() != 6 && hex.size() != 8)
			return 0xFF00FFFF;
		uint32_t v = 0;
		for (char c : hex)
		{
			v <<= 4;
			if (c >= '0' && c <= '9')
				v |= static_cast<uint32_t>(c - '0');
			else if (c >= 'a' && c <= 'f')
				v |= static_cast<uint32_t>(c - 'a' + 10);
			else if (c >= 'A' && c <= 'F')
				v |= static_cast<uint32_t>(c - 'A' + 10);
		}
		if (hex.size() == 6)
			v |= 0xFF000000u;
		return v;
	}

	std::string Attr(const std::string& tag, const char* key)
	{
		std::string k = std::string(key) + "=\"";
		size_t p = tag.find(k);
		if (p == std::string::npos)
		{
			/* case-insensitive key search */
			std::string low = ToLower(tag);
			std::string kl = ToLower(k);
			p = low.find(kl);
			if (p == std::string::npos)
				return {};
		}
		p = tag.find('"', p) + 1;
		size_t e = tag.find('"', p);
		if (e == std::string::npos)
			return {};
		return tag.substr(p, e - p);
	}

	bool ParseBoolValue(const std::string& value, bool fallback = true)
	{
		if (value.empty())
			return fallback;
		const std::string low = ToLower(value);
		if (low == "0" || low == "false" || low == "no")
			return false;
		if (low == "1" || low == "true" || low == "yes")
			return true;
		return fallback;
	}

	void MergeStyle(MarkerStyle& dst, const MarkerStyle& src)
	{
		if (src.hasIconFile) { dst.iconFile = src.iconFile; dst.hasIconFile = true; }
		if (src.hasTexture) { dst.texture = src.texture; dst.hasTexture = true; }
		if (src.hasMinimapVisible) { dst.minimapVisible = src.minimapVisible; dst.hasMinimapVisible = true; }
		if (src.hasMapVisible) { dst.mapVisible = src.mapVisible; dst.hasMapVisible = true; }
		if (src.hasInGameVisible) { dst.inGameVisible = src.inGameVisible; dst.hasInGameVisible = true; }
		if (src.hasMapDisplaySize) { dst.mapDisplaySize = src.mapDisplaySize; dst.hasMapDisplaySize = true; }
		if (src.hasMinSize) { dst.minSize = src.minSize; dst.hasMinSize = true; }
		if (src.hasMaxSize) { dst.maxSize = src.maxSize; dst.hasMaxSize = true; }
		if (src.hasIconSize) { dst.iconSize = src.iconSize; dst.hasIconSize = true; }
		if (src.hasHeightOffset) { dst.heightOffset = src.heightOffset; dst.hasHeightOffset = true; }
		if (src.hasFadeNear) { dst.fadeNear = src.fadeNear; dst.hasFadeNear = true; }
		if (src.hasFadeFar) { dst.fadeFar = src.fadeFar; dst.hasFadeFar = true; }
		if (src.hasAlpha) { dst.alpha = src.alpha; dst.hasAlpha = true; }
		if (src.hasTrailScale) { dst.trailScale = src.trailScale; dst.hasTrailScale = true; }
		if (src.hasColor) { dst.color = src.color; dst.hasColor = true; }
	}

	MarkerStyle ParseStyle(const std::string& tag)
	{
		MarkerStyle out;
		auto compatible = [&](const char* key) {
			std::string value = Attr(tag, key);
			if (value.empty())
				value = Attr(tag, (std::string("bh-") + key).c_str());
			return value;
		};
		std::string value = compatible("iconFile");
		if (!value.empty())
		{
			std::replace(value.begin(), value.end(), '\\', '/');
			out.iconFile = std::move(value);
			out.hasIconFile = true;
		}
		value = compatible("texture");
		if (!value.empty())
		{
			std::replace(value.begin(), value.end(), '\\', '/');
			out.texture = std::move(value);
			out.hasTexture = true;
		}
		value = compatible("minimapVisibility");
		if (!value.empty())
		{
			out.minimapVisible = ParseBoolValue(value);
			out.hasMinimapVisible = true;
		}
		value = compatible("mapVisibility");
		if (!value.empty())
		{
			out.mapVisible = ParseBoolValue(value);
			out.hasMapVisible = true;
		}
		value = compatible("inGameVisibility");
		if (!value.empty())
		{
			out.inGameVisible = ParseBoolValue(value);
			out.hasInGameVisible = true;
		}
		value = compatible("mapDisplaySize");
		if (!value.empty())
		{
			out.mapDisplaySize = static_cast<float>(std::atof(value.c_str()));
			out.hasMapDisplaySize = std::isfinite(out.mapDisplaySize);
		}
		value = compatible("minSize");
		if (!value.empty())
		{
			out.minSize = static_cast<float>(std::atof(value.c_str()));
			out.hasMinSize = std::isfinite(out.minSize);
		}
		value = compatible("maxSize");
		if (!value.empty())
		{
			out.maxSize = static_cast<float>(std::atof(value.c_str()));
			out.hasMaxSize = std::isfinite(out.maxSize);
		}
		value = compatible("iconSize");
		if (!value.empty())
		{
			out.iconSize = static_cast<float>(std::atof(value.c_str()));
			out.hasIconSize = std::isfinite(out.iconSize);
		}
		value = compatible("heightOffset");
		if (!value.empty())
		{
			out.heightOffset = static_cast<float>(std::atof(value.c_str()));
			out.hasHeightOffset = std::isfinite(out.heightOffset);
		}
		value = compatible("fadeNear");
		if (!value.empty())
		{
			out.fadeNear = static_cast<float>(std::atof(value.c_str()));
			out.hasFadeNear = std::isfinite(out.fadeNear);
		}
		value = compatible("fadeFar");
		if (!value.empty())
		{
			out.fadeFar = static_cast<float>(std::atof(value.c_str()));
			out.hasFadeFar = std::isfinite(out.fadeFar);
		}
		value = compatible("alpha");
		if (!value.empty())
		{
			out.alpha = std::clamp(static_cast<float>(std::atof(value.c_str())), 0.f, 1.f);
			out.hasAlpha = true;
		}
		value = compatible("trailScale");
		if (!value.empty())
		{
			out.trailScale = static_cast<float>(std::atof(value.c_str()));
			out.hasTrailScale = std::isfinite(out.trailScale);
		}
		value = Attr(tag, "color");
		if (value.empty()) value = Attr(tag, "bh-color");
		if (value.empty()) value = Attr(tag, "tint");
		if (!value.empty())
		{
			const std::string synthetic = "color=\"" + value + "\"";
			out.color = ParseColorAttr(synthetic);
			out.hasColor = true;
		}
		return out;
	}

	MarkerStyle ResolveStyle(
		const std::string& type,
		const MarkerStyle& own,
		const std::unordered_map<std::string, MarkerStyle>& categories)
	{
		MarkerStyle out;
		const std::string typeLow = ToLower(type);
		size_t pos = 0;
		while (pos < typeLow.size())
		{
			const size_t dot = typeLow.find('.', pos);
			const size_t end = (dot == std::string::npos) ? typeLow.size() : dot;
			const std::string prefix = typeLow.substr(0, end);
			auto it = categories.find(prefix);
			if (it != categories.end())
				MergeStyle(out, it->second);
			if (dot == std::string::npos)
				break;
			pos = dot + 1;
		}
		MergeStyle(out, own);
		return out;
	}

	/* ---- ZIP via miniz (TacO .taco is a zip) ---- */
	bool ReadFileW(const std::wstring& path, std::vector<uint8_t>& out, size_t maxBytes)
	{
		out.clear();
		FILE* f = _wfopen(path.c_str(), L"rb");
		if (!f)
			return false;
		std::fseek(f, 0, SEEK_END);
		const long sz = std::ftell(f);
		std::fseek(f, 0, SEEK_SET);
		if (sz <= 0 || static_cast<size_t>(sz) > maxBytes)
		{
			std::fclose(f);
			return false;
		}
		out.resize(static_cast<size_t>(sz));
		const bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
		std::fclose(f);
		if (!ok)
			out.clear();
		return ok;
	}

	/* Fast lookup via the zip central directory (binary search) instead of a
	   linear scan per trail — critical to avoid a startup freeze with big packs. */
	int ZipLocate(mz_zip_archive& zip, const std::string& entryName)
	{
		std::string want = entryName;
		std::replace(want.begin(), want.end(), '\\', '/');
		int idx = mz_zip_reader_locate_file(&zip, want.c_str(), nullptr, 0);
		if (idx >= 0)
			return idx;
		/* Some packs store backslash separators — try that form too. */
		std::string alt = entryName;
		std::replace(alt.begin(), alt.end(), '/', '\\');
		return mz_zip_reader_locate_file(&zip, alt.c_str(), nullptr, 0);
	}

	bool ZipExtractIndex(mz_zip_archive& zip, int fileIndex,
		std::vector<uint8_t>& out, size_t maxOut)
	{
		out.clear();
		if (fileIndex < 0)
			return false;
		mz_zip_archive_file_stat st{};
		if (!mz_zip_reader_file_stat(&zip, static_cast<mz_uint>(fileIndex), &st) ||
			st.m_is_directory)
			return false;
		if (st.m_uncomp_size == 0 || st.m_uncomp_size > maxOut)
			return false;
		size_t sz = 0;
		void* mem = mz_zip_reader_extract_to_heap(&zip, static_cast<mz_uint>(fileIndex), &sz, 0);
		if (!mem || sz == 0)
			return false;
		out.assign(static_cast<uint8_t*>(mem), static_cast<uint8_t*>(mem) + sz);
		mz_free(mem);
		return true;
	}

	bool ZipExtractEntry(mz_zip_archive& zip, const std::string& entryName,
		std::vector<uint8_t>& out, size_t maxOut)
	{
		return ZipExtractIndex(zip, ZipLocate(zip, entryName), out, maxOut);
	}

	bool ZipReadEntry(const std::wstring& zipPath, const std::string& entryName,
		std::vector<uint8_t>& out, size_t maxOut)
	{
		out.clear();
		std::vector<uint8_t> file;
		if (!ReadFileW(zipPath, file, kMaxZipBytes))
			return false;

		mz_zip_archive zip{};
		if (!mz_zip_reader_init_mem(&zip, file.data(), file.size(), 0))
			return false;
		const bool ok = ZipExtractEntry(zip, entryName, out, maxOut);
		mz_zip_reader_end(&zip);
		return ok;
	}

	/* Read only the .trl header (version + mapId = 8 bytes) via a streaming
	   iterator so we never decompress the whole trail just to learn its map. */
	uint32_t PeekTrlMapId(mz_zip_archive& zip, int fileIndex)
	{
		if (fileIndex < 0)
			return 0;
		mz_zip_reader_extract_iter_state* it =
			mz_zip_reader_extract_iter_new(&zip, static_cast<mz_uint>(fileIndex), 0);
		if (!it)
			return 0;
		uint8_t hdr[8]{};
		const size_t got = mz_zip_reader_extract_iter_read(it, hdr, sizeof(hdr));
		mz_zip_reader_extract_iter_free(it);
		if (got < sizeof(hdr))
			return 0;
		uint32_t mid = 0;
		std::memcpy(&mid, hdr + 4, 4);
		if (mid == 0 || mid > 100000)
			return 0;
		return mid;
	}

	void IndexXml(const std::wstring& packPath, const std::string& xml,
		std::vector<IndexedTrail>& out)
	{
		size_t pos = 0;
		while (pos < xml.size() && out.size() < 30000)
		{
			size_t t = xml.find("<Trail", pos);
			if (t == std::string::npos)
				t = xml.find("<trail", pos);
			if (t == std::string::npos)
				break;
			size_t end = xml.find('>', t);
			if (end == std::string::npos)
				break;
			const std::string tag = xml.substr(t, end - t + 1);
			pos = end + 1;

			std::string data = Attr(tag, "trailData");
			if (data.empty())
				data = Attr(tag, "TrailData");
			if (data.empty())
				continue;
			std::replace(data.begin(), data.end(), '\\', '/');
			while (!data.empty() && (data[0] == '.' || data[0] == '/'))
			{
				if (data.rfind("./", 0) == 0)
					data.erase(0, 2);
				else if (data[0] == '/')
					data.erase(0, 1);
				else
					break;
			}

			IndexedTrail it;
			it.packPath = packPath;
			it.entryName = data;
			it.type = Attr(tag, "type");
			it.color = 0xFFFFFFFFu;
			it.mapCompletion = LooksLikeMapCompletion(it.type, it.entryName);
			it.style = ParseStyle(tag);
			out.push_back(std::move(it));
		}
	}

	void IndexPoisXml(const std::wstring& packPath, const std::string& xml,
		std::vector<IndexedPoi>& out)
	{
		size_t pos = 0;
		while (pos < xml.size() && out.size() < 80000)
		{
			size_t t = xml.find("<POI", pos);
			if (t == std::string::npos)
				t = xml.find("<poi", pos);
			if (t == std::string::npos)
				break;
			/* Require word boundary so we don't match unrelated tags. */
			if (t + 4 < xml.size())
			{
				const char c = xml[t + 4];
				if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '/' && c != '>')
				{
					pos = t + 4;
					continue;
				}
			}
			size_t end = xml.find('>', t);
			if (end == std::string::npos)
				break;
			const std::string tag = xml.substr(t, end - t + 1);
			pos = end + 1;

			std::string type = Attr(tag, "type");
			if (type.empty())
				type = Attr(tag, "Type");
			if (type.empty())
				continue;

			std::string mapStr = Attr(tag, "MapID");
			if (mapStr.empty())
				mapStr = Attr(tag, "mapid");
			if (mapStr.empty())
				mapStr = Attr(tag, "MapId");
			const uint32_t mapId = static_cast<uint32_t>(std::strtoul(mapStr.c_str(), nullptr, 10));
			if (mapId == 0)
				continue;

			std::string xs = Attr(tag, "xpos");
			if (xs.empty())
				xs = Attr(tag, "XPos");
			std::string ys = Attr(tag, "ypos");
			if (ys.empty())
				ys = Attr(tag, "YPos");
			std::string zs = Attr(tag, "zpos");
			if (zs.empty())
				zs = Attr(tag, "ZPos");
			if (xs.empty() || zs.empty())
				continue;

			IndexedPoi poi;
			poi.packPath = packPath;
			poi.type = std::move(type);
			poi.mapId = mapId;
			poi.wx = static_cast<float>(std::atof(xs.c_str()));
			poi.wy = ys.empty() ? 0.f : static_cast<float>(std::atof(ys.c_str()));
			poi.wz = static_cast<float>(std::atof(zs.c_str()));
			poi.style = ParseStyle(tag);
			if (!std::isfinite(poi.wx) || !std::isfinite(poi.wz))
				continue;
			out.push_back(std::move(poi));
		}
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

	void ParseMarkerMenuXml(
		const std::string& xml,
		std::vector<TekkitTrails::Category>& roots,
		std::unordered_map<std::string, MarkerStyle>& styles);
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

	bool ParseTrl(const std::vector<uint8_t>& data, uint32_t& mapId,
		std::vector<TekkitTrails::WorldPoint>& world)
	{
		world.clear();
		if (data.size() < 20)
			return false;
		/* version (u32) + mapId (u32) + N * float3 (x,y,z) — Y up, horizontal = x,z */
		uint32_t ver = 0, mid = 0;
		std::memcpy(&ver, data.data(), 4);
		std::memcpy(&mid, data.data() + 4, 4);
		(void)ver;
		mapId = mid;
		if (mapId == 0 || mapId > 100000)
			return false;

		size_t off = 8;
		size_t rem = data.size() - 8;
		rem -= rem % 12;
		const size_t count = rem / 12;
		if (count < 2)
			return false;

		world.reserve(std::min(count, kMaxPointsPerTrail));
		auto readAt = [&](size_t i, TekkitTrails::WorldPoint& p) -> bool {
			std::memcpy(&p.x, data.data() + off + i * 12, 4);
			std::memcpy(&p.y, data.data() + off + i * 12 + 4, 4);
			std::memcpy(&p.z, data.data() + off + i * 12 + 8, 4);
			if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
				return false;
			if (std::fabs(p.x) > 1.0e6f || std::fabs(p.y) > 1.0e6f || std::fabs(p.z) > 1.0e6f)
				return false;
			return true;
		};
		/* Uniform samples across the ENTIRE .trl — distance-from-start caps used
		   to fill the budget with only the beginning, so paths "stopped" mid-route
		   (Tekkit keeps the full trail). */
		if (count <= kMaxPointsPerTrail)
		{
			for (size_t i = 0; i < count; ++i)
			{
				TekkitTrails::WorldPoint p{};
				if (readAt(i, p))
					world.push_back(p);
			}
		}
		else
		{
			for (size_t k = 0; k < kMaxPointsPerTrail; ++k)
			{
				const size_t i = (k * (count - 1)) / (kMaxPointsPerTrail - 1);
				TekkitTrails::WorldPoint p{};
				if (readAt(i, p))
					world.push_back(p);
			}
		}
		return world.size() >= 2;
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

	/* Parse Tekkit MarkerCategory menu (DisplayName + order) from overlay XML. */
	void ParseMarkerMenuXml(
		const std::string& xml,
		std::vector<TekkitTrails::Category>& roots,
		std::unordered_map<std::string, MarkerStyle>& styles)
	{
		struct Frame
		{
			TekkitTrails::Category* node;
			std::string path;
		};
		std::vector<Frame> stack;
		size_t pos = 0;
		while (pos < xml.size())
		{
			size_t t = xml.find('<', pos);
			if (t == std::string::npos)
				break;
			if (t + 1 < xml.size() && xml[t + 1] == '/')
			{
				size_t end = xml.find('>', t);
				if (end == std::string::npos)
					break;
				const std::string close = ToLower(xml.substr(t + 2, end - (t + 2)));
				if (close.find("markercategory") == 0 && !stack.empty())
					stack.pop_back();
				pos = end + 1;
				continue;
			}

			const bool isCat =
				xml.compare(t, 15, "<MarkerCategory") == 0 ||
				xml.compare(t, 15, "<markercategory") == 0;
			if (!isCat)
			{
				pos = t + 1;
				continue;
			}
			size_t end = xml.find('>', t);
			if (end == std::string::npos)
				break;
			const std::string tag = xml.substr(t, end - t + 1);
			pos = end + 1;

			std::string name = Attr(tag, "name");
			if (name.empty())
				name = Attr(tag, "Name");
			if (name.empty())
				continue;
			std::string display = Attr(tag, "DisplayName");
			if (display.empty())
				display = Attr(tag, "displayname");
			if (display.empty())
				display = name;
			const std::string sepAttr = ToLower(Attr(tag, "IsSeparator"));
			const bool sep = (sepAttr == "1" || sepAttr == "true") ||
				ToLower(name).find("separator") != std::string::npos;

			std::string path = stack.empty() ? name : (stack.back().path + "." + name);
			MarkerStyle style = ParseStyle(tag);
			const std::string stylePath = ToLower(path);
			auto styleIt = styles.find(stylePath);
			if (styleIt == styles.end())
				styles.emplace(stylePath, std::move(style));
			else
				MergeStyle(styleIt->second, style);
			TekkitTrails::Category neu;
			neu.path = path;
			neu.label = display;
			neu.separator = sep;
			std::string hidden = Attr(tag, "IsHidden");
			if (hidden.empty()) hidden = Attr(tag, "bh-IsHidden");
			neu.hidden = ParseBoolValue(hidden, false);
			neu.trails = 0;
			neu.enabled = false;

			std::vector<TekkitTrails::Category>* dest =
				stack.empty() ? &roots : &stack.back().node->children;
			dest->push_back(std::move(neu));
			TekkitTrails::Category* added = &dest->back();

			const bool selfClose = tag.size() >= 2 && tag[tag.size() - 2] == '/';
			if (!selfClose)
				stack.push_back({added, path});
		}
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
	return "addons\\GW2-InGame-Helper\\pathing";
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
