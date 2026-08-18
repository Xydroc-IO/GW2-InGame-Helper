#include "Gw2CatalogInternal.h"

#include "Gw2Http.h"

#include <cstdlib>

using namespace Gw2CatalogDetail;

namespace
{
	RemoteManifest FetchRemoteManifest()
	{
		RemoteManifest remote;
		auto man = Gw2Http::Get(Gw2Catalog::kManifestUrl, nullptr, 8000);
		if (man.ok && ParseManifest(man.body, &remote))
		{
			WriteAll(ManifestPath(), man.body);
			std::lock_guard<std::mutex> lock(gMu);
			if (!remote.catalog.empty())
				gBuild = remote.catalog;
			if (!remote.icons.empty())
				gIconsHash = remote.icons;
		}
		return remote;
	}

	DWORD WINAPI FetchProc(void*)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		TryApplyLocalIgh();

		const RemoteManifest remote = FetchRemoteManifest();
		std::string local;
		bool haveRecipes = false;
		{
			std::lock_guard<std::mutex> lock(gMu);
			local = gBuild;
			haveRecipes = gRecipesOnDisk;
		}
		if (!remote.catalog.empty() && remote.catalog == local && haveRecipes)
		{
			FetchRemoteIcons(remote.icons);
			gBusy = false;
			return 0;
		}

		auto pack = Gw2Http::Get(Gw2Catalog::kPackUrl, nullptr, 60000);
		if (pack.ok && pack.body.size() >= 64)
		{
			WriteAll(PackCachePath(), pack.body);
			ApplyIghBytes(pack.body);
			if (!remote.catalog.empty())
				MergeLocalManifest(remote.catalog.c_str(), nullptr, nullptr);
		}
		FetchRemoteIcons(remote.icons);
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
	if (gLastCheckMs != 0 && (now - gLastCheckMs) < 6u * 60u * 60u * 1000u)
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
