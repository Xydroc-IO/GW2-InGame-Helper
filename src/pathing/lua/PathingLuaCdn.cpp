#include "PathingLuaInternal.h"

#include "Globals.h"
#include "PathingIndex.h"

extern "C" {
#include "lua.h"
}

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <windows.h>
#include <winhttp.h>

namespace PathingLuaDetail
{
	namespace
	{
		std::mutex gCdnMu;
		std::unordered_set<int> gCdnQueued;

		bool HttpGetPng(int assetId, std::vector<uint8_t>& out)
		{
			out.clear();
			wchar_t path[64];
			std::swprintf(path, 64, L"/%d.png", assetId);
			HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/cdn",
				WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
				WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
			if (!session)
				return false;
			WinHttpSetTimeouts(session, 800, 800, 2000, 4000);
			HINTERNET connect = WinHttpConnect(session, L"assets.gw2dat.com",
				INTERNET_DEFAULT_HTTPS_PORT, 0);
			if (!connect)
			{
				WinHttpCloseHandle(session);
				return false;
			}
			HINTERNET request = WinHttpOpenRequest(connect, L"GET", path,
				nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
				WINHTTP_FLAG_SECURE);
			if (!request)
			{
				WinHttpCloseHandle(connect);
				WinHttpCloseHandle(session);
				return false;
			}
			bool ok = false;
			if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
					WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
				WinHttpReceiveResponse(request, nullptr))
			{
				DWORD size = 0;
				do
				{
					size = 0;
					if (!WinHttpQueryDataAvailable(request, &size) || size == 0)
						break;
					if (out.size() + size > 4 * 1024 * 1024)
						break;
					const size_t at = out.size();
					out.resize(at + size);
					DWORD read = 0;
					if (!WinHttpReadData(request, out.data() + at, size, &read))
					{
						out.resize(at);
						break;
					}
					out.resize(at + read);
					ok = true;
				} while (size > 0);
			}
			WinHttpCloseHandle(request);
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
			if (ok && out.size() >= 8 &&
				out[0] == 0x89 && out[1] == 'P' && out[2] == 'N' && out[3] == 'G')
				return true;
			out.clear();
			return false;
		}

		void QueueFetch(int assetId, const std::string& texId)
		{
			{
				std::lock_guard<std::mutex> lock(gCdnMu);
				if (!gCdnQueued.insert(assetId).second)
					return;
			}
			std::thread([assetId, texId]() {
				std::vector<uint8_t> bytes;
				if (!HttpGetPng(assetId, bytes))
					return;
				PathingDetail::PendingIcon pending;
				pending.id = texId;
				pending.bytes = std::move(bytes);
				std::lock_guard<std::mutex> lock(PathingDetail::gIconMutex);
				if (PathingDetail::gPendingIcons.size() >= 256)
					return;
				PathingDetail::gPendingIcons.push_back(std::move(pending));
			}).detach();
		}
	}

	void MakeCdnTextureId(int assetId, char* out, size_t outLen)
	{
		if (!out || outLen == 0)
			return;
		std::snprintf(out, outLen, "GW2IGH_CDN_%d", assetId);
	}

	void RequestCdnTexture(int assetId, char* idOut, size_t idLen)
	{
		if (assetId <= 0 || !idOut || idLen == 0)
			return;
		MakeCdnTextureId(assetId, idOut, idLen);

		if (!G::API)
			return; /* id assigned; upload needs Nexus (or smoke-test stub) */

		if (G::API->Textures_GetOrCreateFromURL)
		{
			char endpoint[64];
			std::snprintf(endpoint, sizeof(endpoint), "/%d.png", assetId);
			G::API->Textures_GetOrCreateFromURL(
				idOut, "https://assets.gw2dat.com", endpoint);
			return;
		}
		/* Fallback: WinHttp -> pending icon upload (same path as pack PNGs). */
		QueueFetch(assetId, idOut);
	}
}
