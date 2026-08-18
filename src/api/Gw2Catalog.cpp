#include "Gw2CatalogInternal.h"

#include "AddonPaths.h"
#include "Gw2Http.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace Gw2CatalogDetail;

namespace
{
	RemoteManifest FetchRemoteManifest()
	{
		RemoteManifest remote;
		auto man = Gw2Http::Get(Gw2Catalog::kManifestUrl, nullptr, 8000);
		if (man.ok)
			ParseManifest(man.body, &remote);
		return remote;
	}

	DWORD WINAPI FetchProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		AddonPaths::CacheDir();
		TryApplyLocalIgh();

		std::string local;
		bool haveRecipes = false;
		bool haveNames = false;
		{
			std::lock_guard<std::mutex> lock(gMu);
			local = gBuild;
			haveRecipes = gRecipesOnDisk;
			auto it = gNames.find('i');
			haveNames = it != gNames.end() && !it->second.empty();
		}
		const RemoteManifest remote = FetchRemoteManifest();
		const bool havePackFile =
			GetFileAttributesW(PackCachePath().c_str()) != INVALID_FILE_ATTRIBUTES;
		/* Never skip the GitHub names pack just because recipes are in memory.
		   Icons can succeed while catalog.igh was never requested. */
		if (!remote.catalog.empty() && remote.catalog == local && haveRecipes && haveNames &&
			havePackFile)
		{
			FetchRemoteIcons(remote.icons);
			FetchRemoteRecipes(remote.catalog);
			FetchRemoteAchievements(remote.catalog);
			gBusy = false;
			return 0;
		}

		auto applyPack = [](std::string bytes) -> bool {
			/* Outer GitHub Content-Encoding gzip only. Members are raw TSV. */
			if (!Gw2Http::GunzipInPlace(bytes))
				return false;
			if (bytes.size() < 64)
				return false;
			if (!ApplyIghBytes(bytes))
				return false;
			WriteAll(PackCachePath(), bytes);
			return true;
		};

		bool got = false;
		if (!havePackFile)
		{
			static std::atomic<bool> sTriedPack;
			if (!sTriedPack.exchange(true))
			{
				const std::wstring tmp = PackCachePath() + L".tmp";
				if (Gw2Http::DownloadToFile(Gw2Catalog::kPackUrl, tmp.c_str(), 120000))
				{
					got = applyPack(ReadAll(tmp, 32u * 1024u * 1024u));
					DeleteFileW(tmp.c_str());
				}
			}
		}
		if (got && !remote.catalog.empty())
			MergeLocalManifest(remote.catalog.c_str(), nullptr, nullptr);
		FetchRemoteIcons(remote.icons);
		FetchRemoteRecipes(remote.catalog);
		FetchRemoteAchievements(remote.catalog);
		gBusy = false;
		return 0;
	}

	bool Lookup(char kind, int id, std::string* out, bool icon)
	{
		if (id <= 0 || !out)
			return false;
		LoadDisk();
		std::lock_guard<std::mutex> lock(gMu);
		return LookupKind(kind, icon ? gIcons : gNames, id, out);
	}
}

void Gw2Catalog::Tick()
{
	LoadDisk();
	const DWORD now = GetTickCount();
	bool haveNames = false;
	bool haveRecipes = false;
	bool haveAch = false;
	{
		std::lock_guard<std::mutex> lock(gMu);
		haveRecipes = gRecipesOnDisk;
		haveAch = gAchievementsOnDisk.load();
		auto it = gNames.find('i');
		haveNames = it != gNames.end() && !it->second.empty();
	}
	if (!haveRecipes &&
		GetFileAttributesW(RecipesPackCachePath().c_str()) != INVALID_FILE_ATTRIBUTES)
		haveRecipes = true;
	if (!haveAch &&
		GetFileAttributesW(AchievementsCachePath().c_str()) != INVALID_FILE_ATTRIBUTES)
		haveAch = true;
	/* Missing names pack: retry every 30 min (not every few seconds — that
	   hammered the GitHub catalog.igh counter). Fresh pack: recheck every 6h. */
	const DWORD waitMs = (haveNames && haveRecipes && haveAch)
		? 6u * 60u * 60u * 1000u
		: 30u * 60u * 1000u;
	if (gLastCheckMs != 0 && (now - gLastCheckMs) < waitMs)
		return;
	if (gBusy.exchange(true))
		return;
	gLastCheckMs = now;
	if (gThread)
	{
		if (WaitForSingleObject(gThread, 0) == WAIT_OBJECT_0)
		{
			CloseHandle(gThread);
			gThread = nullptr;
		}
		else
		{
			gBusy = false;
			return;
		}
	}
	gThread = CreateThread(nullptr, 0, FetchProc, nullptr, 0, nullptr);
	if (!gThread)
		gBusy = false;
}

bool Gw2Catalog::ItemName(int id, std::string* out) { return Name('i', id, out); }
bool Gw2Catalog::CurrencyName(int id, std::string* out) { return Name('c', id, out); }
bool Gw2Catalog::ItemIcon(int id, std::string* out) { return Icon('i', id, out); }
bool Gw2Catalog::CurrencyIcon(int id, std::string* out) { return Icon('c', id, out); }
bool Gw2Catalog::SkinName(int id, std::string* out) { return Name('s', id, out); }
bool Gw2Catalog::SkinIcon(int id, std::string* out) { return Icon('s', id, out); }
bool Gw2Catalog::MiniName(int id, std::string* out) { return Name('n', id, out); }
bool Gw2Catalog::MiniIcon(int id, std::string* out) { return Icon('n', id, out); }
bool Gw2Catalog::MaterialCategoryName(int id, std::string* out) { return Name('m', id, out); }

bool Gw2Catalog::Name(char kind, int id, std::string* out) { return Lookup(kind, id, out, false); }
bool Gw2Catalog::Icon(char kind, int id, std::string* out) { return Lookup(kind, id, out, true); }

bool Gw2Catalog::Extra(char kind, int id, std::string* out)
{
	if (id <= 0 || !out)
		return false;
	LoadDisk();
	std::lock_guard<std::mutex> lock(gMu);
	return LookupKind(kind, gExtra, id, out);
}

static int ClampByte(int n)
{
	if (n < 0)
		return 0;
	if (n > 255)
		return 255;
	return n;
}

bool Gw2Catalog::DyeRgb(int id, int* r, int* g, int* b)
{
	if (!r || !g || !b)
		return false;
	std::string extra;
	if (!Extra('d', id, &extra) || extra.empty())
		return false;
	int rv = 0, gv = 0, bv = 0;
	if (std::sscanf(extra.c_str(), "%d,%d,%d", &rv, &gv, &bv) == 3)
	{
		*r = ClampByte(rv);
		*g = ClampByte(gv);
		*b = ClampByte(bv);
		return true;
	}
	const char* p = extra.c_str();
	if (*p == '#')
		++p;
	unsigned u = 0;
	if (std::strlen(p) == 6 && std::sscanf(p, "%6x", &u) == 1)
	{
		*r = static_cast<int>((u >> 16) & 255);
		*g = static_cast<int>((u >> 8) & 255);
		*b = static_cast<int>(u & 255);
		return true;
	}
	return false;
}

bool Gw2Catalog::HasMaterialCategories()
{
	LoadDisk();
	std::lock_guard<std::mutex> lock(gMu);
	auto it = gNames.find('m');
	return it != gNames.end() && !it->second.empty();
}

bool Gw2Catalog::ArmoryAll(std::vector<ArmoryRow>* out)
{
	if (!out)
		return false;
	LoadDisk();
	std::lock_guard<std::mutex> lock(gMu);
	auto nit = gNames.find('y');
	if (nit == gNames.end() || nit->second.empty())
		return false;
	out->clear();
	out->reserve(nit->second.size());
	auto eit = gExtra.find('y');
	for (const auto& kv : nit->second)
	{
		ArmoryRow row;
		row.id = kv.first;
		row.name = kv.second;
		row.maxCount = 1;
		if (eit != gExtra.end())
		{
			auto x = eit->second.find(kv.first);
			if (x != eit->second.end())
			{
				const int n = std::atoi(x->second.c_str());
				if (n > 0)
					row.maxCount = n;
			}
		}
		out->push_back(std::move(row));
	}
	return !out->empty();
}
