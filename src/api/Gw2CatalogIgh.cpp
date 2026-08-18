#include "Gw2CatalogInternal.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "IghPack.h"

#include <cstring>
#include <mutex>
#include <vector>

using namespace Gw2CatalogDetail;

namespace
{
	constexpr size_t kMaxCatalogIgh = 32u * 1024u * 1024u;
	constexpr size_t kMaxAchIgh = 24u * 1024u * 1024u;
	constexpr size_t kMaxNames = 24u * 1024u * 1024u;
	constexpr size_t kMaxRecipes = 16u * 1024u * 1024u;
	constexpr size_t kMaxAchGroups = 2u * 1024u * 1024u;
	constexpr size_t kMaxAchCats = 4u * 1024u * 1024u;
	constexpr size_t kMaxAchDefs = 16u * 1024u * 1024u;
	constexpr size_t kMaxPng = 2u * 1024u * 1024u;

	std::mutex gIconMu;
	IghPack::Reader gIconPack;

	std::wstring JoinFile(const std::wstring& dir, const wchar_t* name)
	{
		if (dir.empty() || !name || !name[0])
			return {};
		return dir + L"\\" + name;
	}

	std::wstring DllDir()
	{
		wchar_t path[MAX_PATH]{};
		if (G::Self && GetModuleFileNameW(G::Self, path, MAX_PATH))
		{
			std::wstring full = path;
			const size_t slash = full.find_last_of(L"\\/");
			if (slash != std::wstring::npos)
				return full.substr(0, slash);
		}
		return {};
	}

	bool FileExists(const std::wstring& path)
	{
		return !path.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	void TrimLine(std::string& s)
	{
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
			s.pop_back();
	}

	bool OpenIconsPath(const std::wstring& path)
	{
		if (!FileExists(path))
			return false;
		std::lock_guard<std::mutex> lock(gIconMu);
		gIconPack.Close();
		return gIconPack.OpenFile(path.c_str());
	}

	const char* IconKey(const char* renderUrl)
	{
		static constexpr const char kPre[] = "https://render.guildwars2.com/file/";
		if (!renderUrl || std::strncmp(renderUrl, kPre, sizeof(kPre) - 1) != 0)
			return nullptr;
		return renderUrl + (sizeof(kPre) - 1);
	}

	bool LoadAchievementsIgh(const std::string& pack, std::string* groups, std::string* cats,
		std::string* defs, std::string* ver)
	{
		std::string bytes = pack;
		if (!Gw2Http::GunzipInPlace(bytes))
			return false;
		IghPack::Reader r;
		if (!r.OpenBytes(bytes.data(), bytes.size()))
			return false;
		std::string g, c, d, v;
		if (!r.Get("groups.tsv", &g, kMaxAchGroups) ||
			!r.Get("categories.tsv", &c, kMaxAchCats) ||
			!r.Get("defs.tsv", &d, kMaxAchDefs) ||
			!r.Get("ach.ver", &v, 64))
			return false;
		if (g.size() < 16 || c.size() < 16 || d.size() < 16)
			return false;
		TrimLine(v);
		if (groups)
			*groups = std::move(g);
		if (cats)
			*cats = std::move(c);
		if (defs)
			*defs = std::move(d);
		if (ver)
			*ver = std::move(v);
		return true;
	}

	bool LoadAchievementsPath(const std::wstring& path, std::string* groups, std::string* cats,
		std::string* defs, std::string* ver)
	{
		if (!FileExists(path))
			return false;
		return LoadAchievementsIgh(ReadAll(path, kMaxAchIgh), groups, cats, defs, ver);
	}

	std::wstring AchievementsCandidate(int which)
	{
		const wchar_t* name = L"gw2-helper-achievements.igh";
		if (which == 0)
			return JoinFile(AddonPaths::DataDir(), name);
		if (which == 1)
			return AchievementsCachePath();
		return JoinFile(DllDir(), name);
	}
}

bool Gw2CatalogDetail::ApplyIghBytes(const std::string& pack)
{
	IghPack::Reader r;
	if (!r.OpenBytes(pack.data(), pack.size()))
		return false;
	std::string names, recipes, ver;
	if (!r.Get("names-en.tsv", &names, kMaxNames) ||
		!r.Get("recipes.tsv", &recipes, kMaxRecipes) ||
		!r.Get("catalog.ver", &ver, 64))
		return false;
	if (names.size() < 32 || recipes.size() < 16)
		return false;
	ParseTsv(names);
	ParseRecipes(recipes);
	WriteAll(TsvPath(), names);
	WriteAll(RecipesPath(), recipes);
	TrimLine(ver);
	if (!ver.empty())
		MergeLocalManifest(ver.c_str(), nullptr, nullptr);
	return true;
}

bool Gw2CatalogDetail::TryApplyLocalIgh()
{
	const wchar_t* name = L"gw2-helper-catalog.igh";
	const std::wstring cands[] = {
		JoinFile(AddonPaths::DataDir(), name),
		PackCachePath(),
		JoinFile(DllDir(), name),
	};
	for (const std::wstring& p : cands)
	{
		if (!FileExists(p))
			continue;
		const std::string bytes = ReadAll(p, kMaxCatalogIgh);
		if (ApplyIghBytes(bytes))
			return true;
	}
	return false;
}

void Gw2CatalogDetail::FetchRemoteAchievements(const std::string& catalogBuild)
{
	std::string want = catalogBuild;
	TrimLine(want);
	for (int i = 0; i < 3; ++i)
	{
		std::string ver;
		if (!LoadAchievementsPath(AchievementsCandidate(i), nullptr, nullptr, nullptr, &ver))
			continue;
		gAchievementsOnDisk = true;
		if (want.empty() || ver == want)
			return;
	}
	const std::wstring dest = AchievementsCachePath();
	const std::wstring tmp = dest + L".tmp";
	if (!Gw2Http::DownloadToFile(Gw2Catalog::kAchievementsUrl, tmp.c_str(), 120000))
		return;
	std::string bytes = ReadAll(tmp, kMaxAchIgh);
	DeleteFileW(tmp.c_str());
	if (!Gw2Http::GunzipInPlace(bytes) ||
		!LoadAchievementsIgh(bytes, nullptr, nullptr, nullptr, nullptr))
		return;
	if (!WriteAll(dest, bytes))
		return;
	gAchievementsOnDisk = true;
}

bool Gw2Catalog::AchievementPack(std::string* groupsTsv, std::string* categoriesTsv,
	std::string* defsTsv)
{
	if (!groupsTsv || !categoriesTsv || !defsTsv)
		return false;
	for (int i = 0; i < 3; ++i)
	{
		if (LoadAchievementsPath(AchievementsCandidate(i), groupsTsv, categoriesTsv, defsTsv,
			nullptr))
		{
			gAchievementsOnDisk = true;
			return true;
		}
	}
	return false;
}

bool Gw2Catalog::AchievementPackReady()
{
	if (gAchievementsOnDisk.load())
		return true;
	for (int i = 0; i < 3; ++i)
	{
		if (GetFileAttributesW(AchievementsCandidate(i).c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			gAchievementsOnDisk = true;
			return true;
		}
	}
	return false;
}

bool Gw2CatalogDetail::TryOpenLocalIcons()
{
	const wchar_t* name = L"gw2-helper-icons.igh";
	const std::wstring cands[] = {
		JoinFile(AddonPaths::DataDir(), name),
		IconsCachePath(),
		JoinFile(DllDir(), name),
	};
	for (const std::wstring& p : cands)
	{
		if (OpenIconsPath(p))
			return true;
	}
	return false;
}

void Gw2CatalogDetail::FetchRemoteIcons(const std::string& remoteIcons)
{
	std::string remote = remoteIcons;
	TrimLine(remote);
	if (remote.empty())
	{
		TryOpenLocalIcons();
		return;
	}
	std::string local;
	{
		std::lock_guard<std::mutex> lock(gMu);
		local = gIconsHash;
	}
	if (local == remote && FileExists(IconsCachePath()))
	{
		OpenIconsPath(IconsCachePath());
		return;
	}
	const std::wstring tmp = IconsCachePath() + L".tmp";
	if (!Gw2Http::DownloadToFile(Gw2Catalog::kIconsUrl, tmp.c_str(), 180000))
		return;
	{
		std::lock_guard<std::mutex> lock(gIconMu);
		gIconPack.Close();
	}
	DeleteFileW(IconsCachePath().c_str());
	if (!MoveFileW(tmp.c_str(), IconsCachePath().c_str()))
	{
		DeleteFileW(tmp.c_str());
		return;
	}
	MergeLocalManifest(nullptr, remote.c_str(), nullptr);
	OpenIconsPath(IconsCachePath());
}

bool Gw2Catalog::IconPng(const char* renderUrl, std::vector<unsigned char>* out)
{
	const char* key = IconKey(renderUrl);
	if (!key || !out)
		return false;
	std::string blob;
	{
		std::lock_guard<std::mutex> lock(gIconMu);
		if (gIconPack.Has(key) && gIconPack.Get(key, &blob, kMaxPng))
		{
			out->assign(blob.begin(), blob.end());
			return !out->empty();
		}
	}
	if (!TryOpenLocalIcons())
		return false;
	std::lock_guard<std::mutex> lock(gIconMu);
	if (!gIconPack.Get(key, &blob, kMaxPng))
		return false;
	out->assign(blob.begin(), blob.end());
	return !out->empty();
}
