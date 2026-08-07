#include "CraftingData.h"

#include "CraftingShared.h"

#include "Globals.h"
#include "Gw2Http.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace CraftingDetail
{
	static std::mutex gKnownMu;
	static std::unordered_set<int> gAccount;
	static std::unordered_map<std::string, std::unordered_set<int>> gChars;
	static std::unordered_set<int> gPendingAccount;
	static std::unordered_map<std::string, std::unordered_set<int>> gPendingChars;
	static std::unordered_map<int, KnownRecipeInfo> gRecipeDetails;
	static std::atomic<bool> gKnownBusy{false};
	static std::atomic<bool> gKnownReadyFlag{false};
	static std::atomic<bool> gKnownFetched{false};
	static HANDLE gKnownThread = nullptr;
	static DWORD gKnownFetchedAt = 0;

	/* Recipe→output resolve — never on Present. Parallel bulk workers. */
	static std::vector<int> gDetailQueue;
	static std::unordered_set<int> gDetailQueued;
	static std::atomic<bool> gDetailBusy{false};
	static std::atomic<int> gDetailWorkers{0};
	static constexpr size_t kDetailBatch = 100;
	static constexpr int kDetailMaxWorkers = 2;

	static void SaveKnownDisk()
	{
		const std::wstring path = ConfigFile(L"craft_known.txt");
		if (path.empty()) return;
		std::string body = "# craft_known v1\nACCOUNT\n";
		for (int id : gAccount)
			body += std::to_string(id) + "\n";
		for (const auto& kv : gChars)
		{
			body += "CHAR\t";
			body += kv.first;
			body += "\n";
			for (int id : kv.second)
				body += std::to_string(id) + "\n";
		}
		WriteUtf8File(path, body);
	}

	static void SaveKnownDetailsDisk()
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

	static void LoadKnownDetailsDisk()
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

	static void LoadKnownDisk()
	{
		const std::wstring path = ConfigFile(L"craft_known.txt");
		if (path.empty()) return;
		std::string body;
		if (!ReadUtf8File(path, body)) return;
		std::unordered_set<int> account;
		std::unordered_map<std::string, std::unordered_set<int>> chars;
		std::string curChar;
		bool inAccount = false;
		size_t i = 0;
		while (i < body.size())
		{
			size_t e = body.find('\n', i);
			if (e == std::string::npos) e = body.size();
			std::string line = body.substr(i, e - i);
			i = e + 1;
			if (!line.empty() && line.back() == '\r') line.pop_back();
			if (line.empty() || line[0] == '#') continue;
			if (line == "ACCOUNT")
			{
				inAccount = true;
				curChar.clear();
				continue;
			}
			if (line.rfind("CHAR\t", 0) == 0)
			{
				inAccount = false;
				curChar = line.substr(5);
				continue;
			}
			int id = 0;
			if (std::sscanf(line.c_str(), "%d", &id) == 1 && id > 0)
			{
				if (inAccount) account.insert(id);
				else if (!curChar.empty()) chars[curChar].insert(id);
			}
		}
		if (!account.empty() || !chars.empty())
		{
			gAccount = std::move(account);
			gChars = std::move(chars);
			gKnownFetched = true;
		}
		LoadKnownDetailsDisk();
	}

	DWORD WINAPI KnownProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		std::unordered_set<int> account;
		std::unordered_map<std::string, std::unordered_set<int>> chars;

		if (G::Gw2ApiKey[0])
		{
			auto acc = Gw2Http::Api("/v2/account/recipes", G::Gw2ApiKey, kHttpTimeoutMs);
			if (acc.ok)
			{
				std::vector<int> list;
				ParseIntArray(acc.body, list);
				for (int id : list) account.insert(id);
			}

			auto ch = Gw2Http::Api("/v2/characters", G::Gw2ApiKey, kHttpTimeoutMs);
			std::vector<std::string> names;
			if (ch.ok) ParseQuotedStringArray(ch.body, names);
			const size_t maxChars = names.size(); /* all characters — full known set */
			for (size_t i = 0; i < maxChars; ++i)
			{
				std::string path = "/v2/characters/" + EncodeCharPath(names[i]) + "/recipes";
				auto r = Gw2Http::Api(path.c_str(), G::Gw2ApiKey, kHttpTimeoutMs);
				if (!r.ok) continue;
				std::vector<int> list;
				ParseIntArray(r.body, list);
				auto& set = chars[names[i]];
				for (int id : list) set.insert(id);
			}
		}

		{
			std::lock_guard<std::mutex> lock(gKnownMu);
			/* F10-safe: keep prior cache if fetch returned nothing and we had data. */
			if (!account.empty() || !chars.empty() || !gKnownFetched.load())
			{
				gPendingAccount = std::move(account);
				gPendingChars = std::move(chars);
			}
			else
			{
				gPendingAccount = gAccount;
				gPendingChars = gChars;
			}
			gKnownReadyFlag = true;
			gKnownBusy = false;
			gKnownFetchedAt = GetTickCount();
		}
		return 0;
	}

	void StartKnown(bool force)
	{
		static bool loadedDisk = false;
		if (!loadedDisk)
		{
			loadedDisk = true;
			std::vector<int> warm;
			{
				std::lock_guard<std::mutex> lock(gKnownMu);
				LoadKnownDisk(); /* ids + cached recipe details */
				std::unordered_set<int> all = gAccount;
				for (const auto& kv : gChars)
					all.insert(kv.second.begin(), kv.second.end());
				warm.assign(all.begin(), all.end());
			}
			if (!warm.empty())
				EnsureKnownRecipeDetails(warm); /* fills any gaps off-thread */
		}
		if (!force && gKnownFetchedAt != 0 &&
			(GetTickCount() - gKnownFetchedAt) < kKnownTtlMs)
			return;
		if (gKnownBusy.exchange(true)) return;
		if (gKnownThread)
		{
			WaitForSingleObject(gKnownThread, 0);
			CloseHandle(gKnownThread);
			gKnownThread = nullptr;
		}
		gKnownThread = CreateThread(nullptr, 0, KnownProc, nullptr, 0, nullptr);
		if (!gKnownThread) gKnownBusy = false;
	}

	bool KnownBusy() { return gKnownBusy.load(); }

	static DWORD WINAPI KnownSaveDiskProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		std::lock_guard<std::mutex> lock(gKnownMu);
		SaveKnownDisk();
		return 0;
	}

	void KnownTick()
	{
		if (!gKnownReadyFlag) return;
		bool saveDisk = false;
		std::vector<int> prefetch;
		{
			std::lock_guard<std::mutex> lock(gKnownMu);
			if (!gKnownReadyFlag) return;
			gAccount = std::move(gPendingAccount);
			gChars = std::move(gPendingChars);
			gPendingAccount.clear();
			gPendingChars.clear();
			gKnownReadyFlag = false;
			gKnownFetched = true;
			saveDisk = true;
			if (gKnownThread)
			{
				WaitForSingleObject(gKnownThread, 0);
				CloseHandle(gKnownThread);
				gKnownThread = nullptr;
			}
			/* Start resolving names as soon as the id list lands. */
			std::unordered_set<int> all = gAccount;
			for (const auto& kv : gChars)
				all.insert(kv.second.begin(), kv.second.end());
			prefetch.assign(all.begin(), all.end());
		}
		if (saveDisk)
			CreateThread(nullptr, 0, KnownSaveDiskProc, nullptr, 0, nullptr);
		if (!prefetch.empty())
			EnsureKnownRecipeDetails(prefetch);
	}

	bool KnownHasFetched()
	{
		KnownTick();
		return gKnownFetched.load();
	}

	std::vector<std::string> KnownCharacterNames()
	{
		KnownTick();
		std::lock_guard<std::mutex> lock(gKnownMu);
		std::vector<std::string> names;
		names.reserve(gChars.size());
		for (const auto& kv : gChars) names.push_back(kv.first);
		std::sort(names.begin(), names.end());
		return names;
	}

	bool KnownByAccount(int recipeId)
	{
		KnownTick();
		std::lock_guard<std::mutex> lock(gKnownMu);
		return gAccount.count(recipeId) > 0;
	}

	bool CharKnows(const char* charName, int recipeId)
	{
		if (!charName || !charName[0] || recipeId <= 0) return false;
		KnownTick();
		std::lock_guard<std::mutex> lock(gKnownMu);
		auto it = gChars.find(charName);
		return it != gChars.end() && it->second.count(recipeId) > 0;
	}

	std::vector<std::string> CharsKnowing(int recipeId)
	{
		KnownTick();
		std::lock_guard<std::mutex> lock(gKnownMu);
		std::vector<std::string> out;
		for (const auto& kv : gChars)
			if (kv.second.count(recipeId)) out.push_back(kv.first);
		std::sort(out.begin(), out.end());
		return out;
	}

	std::vector<int> KnownRecipeIdsForChar(const char* charName)
	{
		KnownTick();
		std::lock_guard<std::mutex> lock(gKnownMu);
		std::vector<int> out;
		if (charName && charName[0])
		{
			auto it = gChars.find(charName);
			if (it != gChars.end())
				out.assign(it->second.begin(), it->second.end());
		}
		else
		{
			std::unordered_set<int> unionSet = gAccount;
			for (const auto& kv : gChars)
				unionSet.insert(kv.second.begin(), kv.second.end());
			out.assign(unionSet.begin(), unionSet.end());
		}
		std::sort(out.begin(), out.end());
		return out;
	}

	size_t KnownUnionCount()
	{
		return KnownRecipeIdsForChar(nullptr).size();
	}

	int RecipeKnownState(int recipeId, const char* preferChar)
	{
		if (recipeId <= 0) return -2;
		if (!KnownHasFetched())
			return KnownBusy() ? -1 : -1;
		if (preferChar && preferChar[0])
			return CharKnows(preferChar, recipeId) ? 1 : 0;
		if (KnownByAccount(recipeId)) return 1;
		return CharsKnowing(recipeId).empty() ? 0 : 1;
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
