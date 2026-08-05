#include "ProgressDataInternal.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "Settings.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace ProgressDetail
{
	DWORD WINAPI FetchProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		Snapshot snap;
		snap.hasKey = G::Gw2ApiKey[0] != '\0';
		const std::wstring cache = AddonPaths::LiveCacheDir();
		const std::wstring catPath = cache + L"\\live-armory.json";
		const std::wstring namesPath = cache + L"\\live-armory-names.json";

		std::string catalog;
		if (FileFresh(catPath, kArmoryTtlMs))
			catalog = ReadUtf8File(catPath);
		if (catalog.empty() || catalog.find('{') == std::string::npos)
		{
			auto r = Gw2Http::Api("/v2/legendaryarmory?ids=all", nullptr, kBulkTimeoutMs);
			if (r.ok && r.body.find("\"id\"") != std::string::npos)
			{
				catalog = r.body;
				WriteUtf8File(catPath, catalog);
			}
			else
				catalog = ReadUtf8File(catPath);
		}
		ParseArmoryCatalog(catalog, snap.legs);
		if (snap.legs.empty())
		{
			snap.status = "Could not load legendary armory catalog.";
			std::lock_guard<std::mutex> lock(gMu);
			gSnap = std::move(snap);
			gSnap.fetchedAt = GetTickCount();
			++gGen;
			gBusy = false;
			return 0;
		}

		if (FileFresh(namesPath, kArmoryTtlMs))
			ApplyNames(ReadUtf8File(namesPath), snap.legs);
		FetchNames(snap.legs);
		{
			std::string out = "[";
			bool first = true;
			for (const LegRow& r : snap.legs)
			{
				if (r.name.empty()) continue;
				if (!first) out += ',';
				first = false;
				out += "{\"id\":";
				out += std::to_string(r.id);
				out += ",\"name\":\"";
				for (char c : r.name)
				{
					if (c == '"' || c == '\\') out += '\\';
					out += c;
				}
				out += "\"}";
			}
			out += ']';
			if (!first) WriteUtf8File(namesPath, out);
		}

		if (snap.hasKey)
		{
			auto acc = Gw2Http::Api("/v2/account/legendaryarmory", G::Gw2ApiKey, kHttpTimeoutMs);
			if (acc.ok)
			{
				for (LegRow& r : snap.legs) r.owned = 0;
				size_t p = 0;
				while (p < acc.body.size())
				{
					size_t brace = acc.body.find('{', p);
					if (brace == std::string::npos) break;
					size_t end = JsonObjectEnd(acc.body, brace);
					if (end == std::string::npos) break;
					long long id = JsonIntAfterKey(acc.body, "id", brace);
					long long cnt = JsonIntAfterKey(acc.body, "count", brace);
					if (id > 0)
					{
						for (LegRow& r : snap.legs)
						{
							if (r.id == static_cast<int>(id))
							{
								r.owned = cnt > 0 ? static_cast<int>(cnt) : 0;
								break;
							}
						}
					}
					p = end + 1;
				}
				for (const LegRow& r : snap.legs)
					if (r.owned > 0) ++snap.unlocked;
				snap.ok = true;
			}
			else if (acc.status == 401 || acc.status == 403)
			{
				snap.scopeFail = true;
				snap.status = "Need API scopes: account + inventories + unlocks (+ characters).";
			}
			else
				snap.status = "Armory unlocks failed — showing public catalog.";

			auto chars = Gw2Http::Api("/v2/characters", G::Gw2ApiKey, kHttpTimeoutMs);
			if (chars.ok)
			{
				std::vector<std::string> names;
				size_t i = 0;
				while (i < chars.body.size() && names.size() < 64)
				{
					while (i < chars.body.size() && chars.body[i] != '"') ++i;
					if (i >= chars.body.size()) break;
					++i;
					std::string val;
					while (i < chars.body.size() && chars.body[i] != '"')
					{
						if (chars.body[i] == '\\' && i + 1 < chars.body.size())
						{
							val.push_back(chars.body[i + 1]);
							i += 2;
							continue;
						}
						val.push_back(chars.body[i++]);
					}
					if (i < chars.body.size()) ++i;
					if (!val.empty()) names.push_back(val);
				}
				for (const std::string& nm : names)
				{
					CharRow cr;
					cr.name = nm;
					snap.chars.push_back(std::move(cr));
				}
				const size_t detailN = (std::min)(snap.chars.size(), kMaxCharDetails);
				if (detailN > 0)
				{
					std::string path = "/v2/characters?ids=";
					for (size_t ci = 0; ci < detailN; ++ci)
					{
						if (ci) path += ',';
						path += UrlEncodePathSegment(snap.chars[ci].name);
					}
					auto detail = Gw2Http::Api(path.c_str(), G::Gw2ApiKey, kBulkTimeoutMs);
					if (detail.ok)
					{
						size_t p = 0;
						while (p < detail.body.size())
						{
							size_t brace = detail.body.find('{', p);
							if (brace == std::string::npos) break;
							size_t end = JsonObjectEnd(detail.body, brace);
							if (end == std::string::npos) break;
							std::string nm = JsonStringAfterKey(detail.body, "name", brace);
							std::string profession = JsonStringAfterKey(detail.body, "profession", brace);
							long long level = JsonIntAfterKey(detail.body, "level", brace);
							if (!nm.empty())
							{
								for (CharRow& cr : snap.chars)
								{
									if (cr.name == nm)
									{
										cr.profession = profession;
										cr.level = level;
										break;
									}
								}
							}
							p = end + 1;
						}
					}
				}
				if (!snap.ok && !snap.scopeFail)
					snap.ok = true;
			}
			else if (chars.status == 401 || chars.status == 403)
			{
				if (snap.status.empty())
					snap.status = "Character roster needs the characters scope.";
			}
		}
		else
		{
			snap.status = "Public catalog — add an API key for unlocks + roster.";
			snap.ok = !snap.legs.empty();
		}

		if (snap.status.empty())
		{
			char buf[96];
			if (snap.hasKey && !snap.scopeFail)
				std::snprintf(buf, sizeof(buf), "%d / %d unlocked · %d characters",
					snap.unlocked, static_cast<int>(snap.legs.size()),
					static_cast<int>(snap.chars.size()));
			else
				std::snprintf(buf, sizeof(buf), "%d legendary armory items",
					static_cast<int>(snap.legs.size()));
			snap.status = buf;
		}

		snap.fetchedAt = GetTickCount();
		{
			std::lock_guard<std::mutex> lock(gMu);
			gSnap = std::move(snap);
			++gGen;
			gBusy = false;
		}
		return 0;
	}

	void StartFetch(bool force)
	{
		if (!force)
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (gSnap.fetchedAt != 0 && (GetTickCount() - gSnap.fetchedAt) < kAccountTtlMs
				&& !gSnap.legs.empty())
				return;
		}
		if (gBusy.exchange(true))
			return;
		if (gThread)
		{
			WaitForSingleObject(gThread, 0);
			CloseHandle(gThread);
			gThread = nullptr;
		}
		gThread = CreateThread(nullptr, 0, FetchProc, nullptr, 0, nullptr);
		if (!gThread)
			gBusy = false;
	}
} // namespace ProgressDetail
