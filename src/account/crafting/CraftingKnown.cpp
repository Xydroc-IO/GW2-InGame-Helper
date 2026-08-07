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
			std::lock_guard<std::mutex> lock(gKnownMu);
			LoadKnownDisk();
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

	void KnownTick()
	{
		if (!gKnownReadyFlag) return;
		std::lock_guard<std::mutex> lock(gKnownMu);
		if (!gKnownReadyFlag) return;
		gAccount = std::move(gPendingAccount);
		gChars = std::move(gPendingChars);
		gPendingAccount.clear();
		gPendingChars.clear();
		gKnownReadyFlag = false;
		gKnownFetched = true;
		SaveKnownDisk();
		if (gKnownThread)
		{
			WaitForSingleObject(gKnownThread, 0);
			CloseHandle(gKnownThread);
			gKnownThread = nullptr;
		}
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

	void EnsureKnownRecipeDetails(const std::vector<int>& recipeIds)
	{
		std::vector<int> need;
		{
			std::lock_guard<std::mutex> lock(gKnownMu);
			for (int id : recipeIds)
				if (id > 0 && !gRecipeDetails.count(id))
					need.push_back(id);
		}
		if (need.empty()) return;
		const size_t maxN = (std::min)(need.size(), size_t{40});
		for (size_t off = 0; off < maxN; off += 20)
		{
			const size_t n = (std::min)(maxN - off, size_t{20});
			std::string path = "/v2/recipes?ids=";
			for (size_t i = 0; i < n; ++i)
			{
				if (i) path += ',';
				path += std::to_string(need[off + i]);
			}
			auto r = Gw2Http::Api(path.c_str(), nullptr, kBulkTimeoutMs);
			if (!r.ok) continue;
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
				if (info.recipeId > 0 && info.outputId > 0)
				{
					info.outputName = ItemName(info.outputId);
					std::lock_guard<std::mutex> lock(gKnownMu);
					gRecipeDetails[info.recipeId] = info;
				}
				p = end + 1;
			}
		}
		std::vector<int> outIds;
		{
			std::lock_guard<std::mutex> lock(gKnownMu);
			for (int id : need)
			{
				auto it = gRecipeDetails.find(id);
				if (it != gRecipeDetails.end() && it->second.outputName.empty())
					outIds.push_back(it->second.outputId);
			}
		}
		if (!outIds.empty())
		{
			std::unordered_map<int, std::string> names;
			FetchNames(names, outIds);
			std::lock_guard<std::mutex> lock(gKnownMu);
			for (auto& kv : gRecipeDetails)
			{
				auto nit = names.find(kv.second.outputId);
				if (nit != names.end()) kv.second.outputName = nit->second;
			}
		}
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
