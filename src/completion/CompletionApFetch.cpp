#include "CompletionShared.h"
#include "CompletionInternal.h"
#include "CompletionApIds.h"

#include "Globals.h"
#include "Gw2Http.h"
#include "Settings.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace CompletionDetail
{
	namespace
	{
		constexpr int kHttpTimeoutMs = 4000;
		std::mutex gMu;
		std::unordered_map<uint32_t, ApProgress> gProgress;
		std::string gNote;
		std::atomic<bool> gBusy{ false };
		std::atomic<bool> gReady{ false };
		HANDLE gThread = nullptr;
		std::unordered_map<uint32_t, ApProgress> gPend;
		std::string gPendNote;

		void CollectIds(const std::string& body, std::unordered_map<uint32_t, ApProgress>& out)
		{
			size_t pos = 0;
			while (pos < body.size())
			{
				const size_t idKey = body.find("\"id\"", pos);
				if (idKey == std::string::npos)
					break;
				const size_t colon = body.find(':', idKey);
				if (colon == std::string::npos || colon > idKey + 8)
				{
					pos = idKey + 4;
					continue;
				}
				char* end = nullptr;
				const unsigned long id = std::strtoul(body.c_str() + colon + 1, &end, 10);
				if (!end || id == 0 || id > 10000000ul)
				{
					pos = idKey + 4;
					continue;
				}
				size_t blockEnd = body.find('{', static_cast<size_t>(end - body.c_str()));
				/* Prefer next object start; fall back to +400 chars. */
				if (blockEnd == std::string::npos || blockEnd > idKey + 500)
					blockEnd = (idKey + 500 < body.size()) ? idKey + 500 : body.size();
				const std::string slice = body.substr(idKey, blockEnd - idKey);
				ApProgress p{};
				p.achievementId = static_cast<uint32_t>(id);
				p.known = true;
				if (slice.find("\"done\":true") != std::string::npos ||
					slice.find("\"done\": true") != std::string::npos)
					p.done = true;
				const size_t cur = slice.find("\"current\"");
				if (cur != std::string::npos)
				{
					const size_t c2 = slice.find(':', cur);
					if (c2 != std::string::npos)
						p.current = static_cast<int>(std::strtol(slice.c_str() + c2 + 1, nullptr, 10));
				}
				const size_t mx = slice.find("\"max\"");
				if (mx != std::string::npos)
				{
					const size_t m2 = slice.find(':', mx);
					if (m2 != std::string::npos)
						p.max = static_cast<int>(std::strtol(slice.c_str() + m2 + 1, nullptr, 10));
				}
				out[p.achievementId] = p;
				pos = idKey + 4;
			}
		}

		DWORD WINAPI Worker(LPVOID)
		{
			std::unordered_map<uint32_t, ApProgress> local;
			std::string note;
			if (!G::Gw2ApiKey[0])
			{
				note = "API key required for achievement overlay.";
			}
			else
			{
				auto r = Gw2Http::Api("/v2/account/achievements", G::Gw2ApiKey, kHttpTimeoutMs);
				if (!r.ok || r.body.empty())
					note = "Could not load /v2/account/achievements.";
				else
				{
					CollectIds(r.body, local);
					char buf[80];
					std::snprintf(buf, sizeof(buf),
						"API overlay: %d achievements on account.",
						static_cast<int>(local.size()));
					note = buf;
				}
			}
			{
				std::lock_guard<std::mutex> lock(gMu);
				gPend = std::move(local);
				gPendNote = std::move(note);
				gReady = true;
			}
			gBusy = false;
			return 0;
		}
	}

	void BeginApOverlayRefresh()
	{
		if (gBusy.exchange(true))
			return;
		if (gThread)
		{
			WaitForSingleObject(gThread, 0);
			CloseHandle(gThread);
			gThread = nullptr;
		}
		gThread = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
		if (!gThread)
			gBusy = false;
	}

	void ApplyApOverlayResult()
	{
		if (!gReady.load())
			return;
		std::lock_guard<std::mutex> lock(gMu);
		if (!gReady)
			return;
		gProgress = std::move(gPend);
		gNote = std::move(gPendNote);
		gReady = false;
		if (gNote.size() < sizeof(gStatus))
			std::snprintf(gStatus, sizeof(gStatus), "%s", gNote.c_str());
		if (gThread)
		{
			WaitForSingleObject(gThread, 0);
			CloseHandle(gThread);
			gThread = nullptr;
		}
	}

	bool ApOverlayBusy()
	{
		return gBusy.load();
	}

	bool LookupApProgress(uint32_t achievementId, ApProgress& out)
	{
		std::lock_guard<std::mutex> lock(gMu);
		const auto it = gProgress.find(achievementId);
		if (it == gProgress.end())
			return false;
		out = it->second;
		return true;
	}

	bool FormatApOverlayLine(uint32_t mapId, const char* packType, char* out, size_t outLen)
	{
		if (!out || outLen == 0)
			return false;
		out[0] = '\0';
		const ApIdRow* row = nullptr;
		if (packType && packType[0])
			row = ApIdForPackPrefix(packType);
		if (!row)
			row = ApIdForMap(mapId);
		if (!row)
			return false;
		ApProgress p{};
		if (!LookupApProgress(row->achievementId, p))
		{
			std::snprintf(out, outLen, "API: %s (refresh / key)", row->label);
			return true;
		}
		if (p.done)
			std::snprintf(out, outLen, "API: %s [done]", row->label);
		else if (p.max > 0)
			std::snprintf(out, outLen, "API: %s %d/%d", row->label, p.current, p.max);
		else
			std::snprintf(out, outLen, "API: %s [in progress]", row->label);
		return true;
	}
}
