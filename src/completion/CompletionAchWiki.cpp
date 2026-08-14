#include "CompletionShared.h"

#include "Gw2Http.h"
#include "WikiBrowser.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <windows.h>

namespace CompletionDetail
{
	namespace
	{
		std::mutex gMu;
		std::unordered_map<int, std::string> gUrl;
		std::unordered_set<int> gTried;
		std::atomic<bool> gBusy{ false };
		int gPendId = 0;
		std::string gPendUrl;
		std::atomic<bool> gReady{ false };

		std::string ParseThumbUrl(const std::string& body)
		{
			const size_t src = body.find("\"source\"");
			if (src == std::string::npos)
				return {};
			const size_t q = body.find('"', src + 8);
			if (q == std::string::npos)
				return {};
			const size_t q2 = body.find('"', q + 1);
			if (q2 == std::string::npos || q2 <= q + 1)
				return {};
			std::string url = body.substr(q + 1, q2 - q - 1);
			if (url.rfind("https://wiki.guildwars2.com/", 0) != 0)
				return {};
			return url;
		}

		struct Job
		{
			int id = 0;
			char name[160]{};
		};

		DWORD WINAPI Worker(LPVOID arg)
		{
			Job* job = static_cast<Job*>(arg);
			std::string url;
			if (job && job->name[0])
			{
				std::string title = job->name;
				for (char& c : title)
				{
					if (c == ' ')
						c = '_';
				}
				const std::string enc = WikiBrowser::UrlEncode(title);
				std::string api =
					"https://wiki.guildwars2.com/api.php?action=query&format=json"
					"&formatversion=2&prop=pageimages&pithumbsize=360&titles=";
				api += enc;
				auto r = Gw2Http::Get(api.c_str(), nullptr, 8000);
				if (r.ok)
					url = ParseThumbUrl(r.body);
			}
			{
				std::lock_guard<std::mutex> lock(gMu);
				gPendId = job ? job->id : 0;
				gPendUrl = std::move(url);
				gReady = true;
			}
			delete job;
			gBusy = false;
			return 0;
		}
	}

	void BeginAchWikiThumb(int achievementId, const char* name)
	{
		if (achievementId <= 0 || !name || !name[0])
			return;
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (gTried.count(achievementId) || gUrl.count(achievementId))
				return;
			if (gBusy.load())
				return;
			gTried.insert(achievementId);
		}
		if (gBusy.exchange(true))
			return;
		Job* job = new Job();
		job->id = achievementId;
		std::snprintf(job->name, sizeof(job->name), "%s", name);
		HANDLE th = CreateThread(nullptr, 0, Worker, job, 0, nullptr);
		if (!th)
		{
			delete job;
			gBusy = false;
			std::lock_guard<std::mutex> lock(gMu);
			gTried.erase(achievementId);
			return;
		}
		CloseHandle(th);
	}

	void ApplyAchWikiThumbResult()
	{
		if (!gReady.load())
			return;
		std::lock_guard<std::mutex> lock(gMu);
		if (!gReady)
			return;
		if (gPendId > 0)
			gUrl[gPendId] = std::move(gPendUrl);
		gPendUrl.clear();
		gPendId = 0;
		gReady = false;
	}

	bool LookupAchWikiThumbUrl(int achievementId, std::string& outUrl)
	{
		std::lock_guard<std::mutex> lock(gMu);
		const auto it = gUrl.find(achievementId);
		if (it == gUrl.end() || it->second.empty())
			return false;
		outUrl = it->second;
		return true;
	}
}
