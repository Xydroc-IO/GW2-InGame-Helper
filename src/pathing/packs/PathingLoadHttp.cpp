#include "PathingTrails.h"

#include "Globals.h"
#include "PathingIndex.h"
#include "PathingParse.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#include <windows.h>
#include <winhttp.h>

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
		   to a few pixels near the map center - the "blob" bug. */
		constexpr float kMetersToInches = 39.3700787f;
		const float wx = wxMeters * kMetersToInches;
		const float wz = wzMeters * kMetersToInches;

		const float tx = (wx - r.mx0) / (r.mx1 - r.mx0);
		/* Same transform as the classic Mumble->continent formula:
		   continent_y uses -world_z against map_rect.y. */
		const float ty = (-wz - r.my0) / (r.my1 - r.my0);
		cx = r.cx0 + tx * (r.cx1 - r.cx0);
		cy = r.cy0 + ty * (r.cy1 - r.cy0);
	}

	void ContinentToWorld(const Rects& r, float cx, float cy, float& wxMeters, float& wzMeters)
	{
		constexpr float kInchesToMeters = 1.f / 39.3700787f;
		const float tx = (cx - r.cx0) / (r.cx1 - r.cx0);
		const float ty = (cy - r.cy0) / (r.cy1 - r.cy0);
		const float wxIn = r.mx0 + tx * (r.mx1 - r.mx0);
		const float wzIn = -(r.my0 + ty * (r.my1 - r.my0));
		wxMeters = wxIn * kInchesToMeters;
		wzMeters = wzIn * kInchesToMeters;
	}


} // namespace PathingDetail
