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
	constexpr size_t kMaxNames = 24u * 1024u * 1024u;
	constexpr size_t kMaxRecipes = 16u * 1024u * 1024u;
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
	{
		WriteAll(VerPath(), ver + "\n");
		std::lock_guard<std::mutex> lock(gMu);
		gBuild = std::move(ver);
	}
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

void Gw2CatalogDetail::FetchRemoteIcons()
{
	auto ver = Gw2Http::Get(Gw2Catalog::kIconsVerUrl, nullptr, 8000);
	std::string remote;
	if (ver.ok && !ver.body.empty())
	{
		remote = ver.body;
		TrimLine(remote);
	}
	if (remote.empty())
		return;
	std::string local = ReadAll(IconsVerPath(), 128);
	TrimLine(local);
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
	WriteAll(IconsVerPath(), remote + "\n");
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
