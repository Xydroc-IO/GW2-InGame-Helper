#include "CraftingData.h"

#include "CraftingKnownInternal.h"
#include "CraftingShared.h"

#include "Gw2Http.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace CraftingDetail
{
	std::unordered_map<int, KnownRecipeInfo> gRecipeDetails;
	std::vector<int> gDetailQueue;
	std::unordered_set<int> gDetailQueued;
	std::atomic<bool> gDetailBusy{false};
	std::atomic<int> gDetailWorkers{0};

	void SaveKnownDetailsDisk()
	{
		const std::wstring path = ConfigFile(L"craft_recipe_details.txt");
		if (path.empty()) return;
		std::unordered_map<int, KnownRecipeInfo> snap;
		{
			std::lock_guard<std::mutex> lock(gKnownMu);
			snap = gRecipeDetails;
		}
		std::string body = "# craft_recipe_details v1\n";
		body.reserve(snap.size() * 48);
		for (const auto& kv : snap)
		{
			const KnownRecipeInfo& info = kv.second;
			body += std::to_string(info.recipeId);
			body += '\t';
			body += std::to_string(info.outputId);
			body += '\t';
			body += std::to_string(info.outCount);
			body += '\t';
			body += info.discipline;
			body += '\t';
			body += info.outputName;
			body += '\n';
		}
		WriteUtf8File(path, body);
	}

	void LoadKnownDetailsDisk()
	{
		const std::wstring path = ConfigFile(L"craft_recipe_details.txt");
		if (path.empty()) return;
		std::string body;
		if (!ReadUtf8File(path, body)) return;
		size_t i = 0;
		while (i < body.size())
		{
			size_t e = body.find('\n', i);
			if (e == std::string::npos) e = body.size();
			std::string line = body.substr(i, e - i);
			i = e + 1;
			if (!line.empty() && line.back() == '\r') line.pop_back();
			if (line.empty() || line[0] == '#') continue;
			/* recipeId \t outputId \t outCount \t discipline \t name */
			int recipeId = 0, outputId = 0, outCount = 1;
			size_t t1 = line.find('\t');
			if (t1 == std::string::npos) continue;
			size_t t2 = line.find('\t', t1 + 1);
			if (t2 == std::string::npos) continue;
			size_t t3 = line.find('\t', t2 + 1);
			if (t3 == std::string::npos) continue;
			size_t t4 = line.find('\t', t3 + 1);
			if (t4 == std::string::npos) continue;
			if (std::sscanf(line.c_str(), "%d", &recipeId) != 1 || recipeId <= 0) continue;
			if (std::sscanf(line.c_str() + t1 + 1, "%d", &outputId) != 1 || outputId <= 0) continue;
			std::sscanf(line.c_str() + t2 + 1, "%d", &outCount);
			if (outCount <= 0) outCount = 1;
			KnownRecipeInfo info;
			info.recipeId = recipeId;
			info.outputId = outputId;
			info.outCount = outCount;
			info.discipline = line.substr(t3 + 1, t4 - t3 - 1);
			info.outputName = line.substr(t4 + 1);
			gRecipeDetails[recipeId] = std::move(info);
		}
	}

	static KnownRecipeInfo MakeDetailStub(int recipeId)
	{
		KnownRecipeInfo info;
		info.recipeId = recipeId;
		info.outputId = 0;
		info.outCount = 1;
		info.discipline = "Other";
		info.outputName = "(unavailable)";
		return info;
	}

	static void FetchDetailBatch(const std::vector<int>& batch)
	{
		if (batch.empty()) return;
		std::string path = "/v2/recipes?ids=";
		for (size_t i = 0; i < batch.size(); ++i)
		{
			if (i) path += ',';
			path += std::to_string(batch[i]);
		}

		Gw2Http::Result r;
		bool gotBody = false;
		for (int attempt = 0; attempt < 6; ++attempt)
		{
			r = Gw2Http::Api(path.c_str(), nullptr, kBulkTimeoutMs);
			if (r.ok)
			{
				gotBody = true;
				break;
			}
			/* Every id invalid — mark stubs so the UI can finish. */
			if (r.status == 404)
			{
				std::lock_guard<std::mutex> lock(gKnownMu);
				for (int id : batch)
				{
					if (id > 0 && !gRecipeDetails.count(id))
						gRecipeDetails[id] = MakeDetailStub(id);
				}
				return;
			}
			/* 429 / transient — back off before the Present loop re-kicks workers. */
			const DWORD ms = (r.status == 429)
				? (1500u + 750u * static_cast<DWORD>(attempt))
				: (400u << (attempt < 3 ? attempt : 3));
			Sleep(ms);
		}

		if (!gotBody)
		{
			/* Re-queue so a later worker pass can try again after the bucket refills. */
			{
				std::lock_guard<std::mutex> lock(gKnownMu);
				for (int id : batch)
				{
					if (id <= 0 || gRecipeDetails.count(id) || gDetailQueued.count(id))
						continue;
					gDetailQueue.push_back(id);
					gDetailQueued.insert(id);
				}
			}
			Sleep(2500);
			return;
		}

		std::vector<KnownRecipeInfo> parsed;
		std::vector<int> outIds;
		size_t p = 0;
		while (p < r.body.size())
		{
			size_t brace = r.body.find('{', p);
			if (brace == std::string::npos) break;
			size_t end = JsonObjectEnd(r.body, brace);
			if (end == std::string::npos) break;
			KnownRecipeInfo info;
			info.recipeId = static_cast<int>(JsonIntAfterKey(r.body, "id", brace));
			info.outputId = static_cast<int>(JsonIntAfterKey(r.body, "output_item_id", brace));
			info.outCount = static_cast<int>(JsonIntAfterKey(r.body, "output_item_count", brace));
			if (info.outCount <= 0) info.outCount = 1;
			size_t dkey = r.body.find("\"disciplines\"", brace);
			if (dkey != std::string::npos && dkey < end)
			{
				size_t bracket = r.body.find('[', dkey);
				size_t q1 = (bracket == std::string::npos) ? std::string::npos : r.body.find('"', bracket);
				if (q1 != std::string::npos && q1 < end)
				{
					++q1;
					size_t q2 = r.body.find('"', q1);
					if (q2 != std::string::npos && q2 < end)
						info.discipline = r.body.substr(q1, q2 - q1);
				}
			}
			if (info.recipeId > 0)
			{
				if (info.outputId > 0)
				{
					parsed.push_back(info);
					outIds.push_back(info.outputId);
				}
				else
				{
					info.discipline = info.discipline.empty() ? "Other" : info.discipline;
					info.outputName = "(unavailable)";
					parsed.push_back(std::move(info));
				}
			}
			p = end + 1;
		}

		std::unordered_map<int, std::string> names;
		if (!outIds.empty())
			FetchNames(names, outIds);
		for (KnownRecipeInfo& info : parsed)
		{
			auto nit = names.find(info.outputId);
			if (nit != names.end())
				info.outputName = nit->second;
		}

		{
			std::lock_guard<std::mutex> lock(gKnownMu);
			for (KnownRecipeInfo& info : parsed)
				gRecipeDetails[info.recipeId] = std::move(info);
			/* 206 / missing ids — stub so we never spin forever on dead recipe ids. */
			for (int id : batch)
			{
				if (id > 0 && !gRecipeDetails.count(id))
					gRecipeDetails[id] = MakeDetailStub(id);
			}
		}
	}

	static void KickDetailWorkers();

	static DWORD WINAPI KnownDetailProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
		for (;;)
		{
			std::vector<int> batch;
			{
				std::lock_guard<std::mutex> lock(gKnownMu);
				while (!gDetailQueue.empty() && batch.size() < kDetailBatch)
				{
					const int id = gDetailQueue.back();
					gDetailQueue.pop_back();
					gDetailQueued.erase(id);
					if (id > 0 && !gRecipeDetails.count(id))
						batch.push_back(id);
				}
				if (batch.empty())
				{
					const int left = --gDetailWorkers;
					if (left <= 0)
					{
						gDetailWorkers = 0;
						gDetailBusy = false;
						SaveKnownDetailsDisk();
					}
					return 0;
				}
			}
			FetchDetailBatch(batch);
		}
	}

	static void KickDetailWorkers()
	{
		for (;;)
		{
			int cur = gDetailWorkers.load();
			if (cur >= kDetailMaxWorkers)
				return;
			{
				std::lock_guard<std::mutex> lock(gKnownMu);
				if (gDetailQueue.empty())
					return;
			}
			if (!gDetailWorkers.compare_exchange_weak(cur, cur + 1))
				continue;
			gDetailBusy = true;
			HANDLE h = CreateThread(nullptr, 0, KnownDetailProc, nullptr, 0, nullptr);
			if (!h)
			{
				if (--gDetailWorkers <= 0)
				{
					gDetailWorkers = 0;
					gDetailBusy = false;
				}
				return;
			}
			CloseHandle(h); /* fire-and-forget; thread keeps running */
		}
	}

	void EnsureKnownRecipeDetails(const std::vector<int>& recipeIds)
	{
		{
			std::lock_guard<std::mutex> lock(gKnownMu);
			for (int id : recipeIds)
			{
				if (id <= 0 || gRecipeDetails.count(id) || gDetailQueued.count(id))
					continue;
				gDetailQueue.push_back(id);
				gDetailQueued.insert(id);
			}
		}
		KickDetailWorkers();
	}

	void EnsureNextKnownRecipeDetails(const std::vector<int>& recipeIds, size_t maxN)
	{
		if (maxN == 0 || recipeIds.empty()) return;
		std::vector<int> batch;
		batch.reserve(maxN);
		{
			std::lock_guard<std::mutex> lock(gKnownMu);
			for (int id : recipeIds)
			{
				if (id <= 0 || gRecipeDetails.count(id) || gDetailQueued.count(id))
					continue;
				batch.push_back(id);
				if (batch.size() >= maxN)
					break;
			}
		}
		if (!batch.empty())
			EnsureKnownRecipeDetails(batch);
	}

	size_t KnownDetailsReadyCount(const std::vector<int>& recipeIds)
	{
		std::lock_guard<std::mutex> lock(gKnownMu);
		size_t n = 0;
		for (int id : recipeIds)
			if (id > 0 && gRecipeDetails.count(id))
				++n;
		return n;
	}

	/* One lock — copy available details for UI (avoids N× GetKnownRecipeDetail). */
	void CopyKnownRecipeDetails(const std::vector<int>& recipeIds,
		std::vector<KnownRecipeInfo>& out, size_t* readyOut)
	{
		out.clear();
		std::lock_guard<std::mutex> lock(gKnownMu);
		size_t ready = 0;
		out.reserve(recipeIds.size());
		for (int id : recipeIds)
		{
			if (id <= 0) continue;
			auto it = gRecipeDetails.find(id);
			if (it == gRecipeDetails.end()) continue;
			++ready;
			out.push_back(it->second);
		}
		if (readyOut) *readyOut = ready;
	}

	bool GetKnownRecipeDetail(int recipeId, KnownRecipeInfo& out)
	{
		std::lock_guard<std::mutex> lock(gKnownMu);
		auto it = gRecipeDetails.find(recipeId);
		if (it == gRecipeDetails.end()) return false;
		out = it->second;
		return true;
	}

} // namespace CraftingDetail
