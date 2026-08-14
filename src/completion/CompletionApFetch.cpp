#include "CompletionShared.h"
#include "CompletionInternal.h"
#include "CompletionApIds.h"

#include "Globals.h"
#include "Gw2Http.h"
#include "JsonView.h"
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
		constexpr int kHttpTimeoutMs = 12000;
		std::mutex gMu;
		std::unordered_map<uint32_t, ApProgress> gProgress;
		std::string gNote;
		std::atomic<bool> gBusy{ false };
		std::atomic<bool> gReady{ false };
		HANDLE gThread = nullptr;
		std::unordered_map<uint32_t, ApProgress> gPend;
		std::string gPendNote;
		DWORD gLastFetchMs = 0;
		constexpr DWORD kMinRefetchMs = 180u * 1000u;

		void CollectBitIndices(const std::string& json, size_t openBracket, size_t limit,
			std::vector<int>& out)
		{
			if (openBracket >= json.size() || json[openBracket] != '[')
				return;
			size_t k = openBracket + 1;
			while (k < limit && k < json.size() && json[k] != ']')
			{
				int idx = -1;
				size_t after = k;
				if (JsonView::ParseInt32(JsonView::AsView(json), k, &idx, &after) && idx >= 0)
					out.push_back(idx);
				k = (after > k) ? after : k + 1;
				while (k < limit && k < json.size() && json[k] != ']' &&
					(json[k] < '0' || json[k] > '9') && json[k] != '-')
					++k;
			}
		}

		void CollectIds(const std::string& body, std::unordered_map<uint32_t, ApProgress>& out)
		{
			size_t pos = 0;
			while (pos < body.size())
			{
				const size_t brace = body.find('{', pos);
				if (brace == std::string::npos)
					break;
				const size_t end = JsonView::ObjectEnd(body, brace);
				if (end == std::string::npos)
					break;
				const long long id = JsonView::IntAfterKey(body, "id", brace);
				if (id > 0 && id <= 10000000ll)
				{
					ApProgress p{};
					p.achievementId = static_cast<uint32_t>(id);
					p.known = true;
					const std::string slice = body.substr(brace, end - brace + 1);
					if (slice.find("\"done\":true") != std::string::npos ||
						slice.find("\"done\": true") != std::string::npos)
						p.done = true;
					const long long cur = JsonView::IntAfterKey(body, "current", brace);
					if (cur > 0)
						p.current = static_cast<int>(cur);
					const long long mx = JsonView::IntAfterKey(body, "max", brace);
					if (mx > 0)
						p.max = static_cast<int>(mx);
					const size_t vs = JsonView::ValueStartAfterKey(JsonView::AsView(body),
						JsonView::View("bits"), brace);
					if (vs != JsonView::View::npos && vs < end && body[vs] == '[')
						CollectBitIndices(body, vs, end, p.bits);
					out[p.achievementId] = std::move(p);
				}
				pos = end + 1;
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
		const DWORD now = GetTickCount();
		/* Skip reopen storms — account achievements payload is large. */
		if (gLastFetchMs != 0 && (now - gLastFetchMs) < kMinRefetchMs)
			return;
		if (gBusy.exchange(true))
			return;
		if (gThread)
		{
			WaitForSingleObject(gThread, 0);
			CloseHandle(gThread);
			gThread = nullptr;
		}
		gLastFetchMs = now;
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

	size_t ApProgressCount()
	{
		std::lock_guard<std::mutex> lock(gMu);
		return gProgress.size();
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
