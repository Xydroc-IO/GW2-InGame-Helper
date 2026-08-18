#include "Gw2CatalogInternal.h"

#include "AddonPaths.h"
#include "JsonView.h"

#include <cstdlib>
#include <cstring>
#include <utility>

namespace Gw2CatalogDetail
{
	std::mutex gMu;
	std::unordered_map<char, std::unordered_map<int, std::string>> gNames;
	std::unordered_map<char, std::unordered_map<int, std::string>> gIcons;
	std::unordered_map<char, std::unordered_map<int, std::string>> gExtra;
	std::unordered_map<int, Gw2Catalog::Recipe> gRecipes;
	std::unordered_map<int, std::vector<int>> gByOutput;
	std::string gBuild;
	std::string gIconsHash;
	bool gDiskLoaded = false;
	bool gRecipesOnDisk = false;
	std::atomic<bool> gBusy{false};
	HANDLE gThread = nullptr;
	DWORD gLastCheckMs = 0;

	constexpr const char kRenderPrefix[] = "https://render.guildwars2.com/file/";

	std::wstring ManifestPath()
	{
		return AddonPaths::CacheDir() + L"\\gw2-helper-catalog.manifest";
	}
	std::wstring TsvPath() { return AddonPaths::CacheDir() + L"\\gw2-names-en.tsv"; }
	std::wstring RecipesPath() { return AddonPaths::CacheDir() + L"\\gw2-recipes.tsv"; }
	std::wstring PackCachePath()
	{
		return AddonPaths::CacheDir() + L"\\gw2-helper-catalog.igh";
	}
	std::wstring IconsCachePath()
	{
		return AddonPaths::CacheDir() + L"\\gw2-helper-icons.igh";
	}

	std::string ReadAll(const std::wstring& path, size_t maxBytes)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return {};
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 ||
			static_cast<size_t>(sz.QuadPart) > maxBytes)
		{
			CloseHandle(h);
			return {};
		}
		std::string data(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD rd = 0;
		ReadFile(h, data.data(), static_cast<DWORD>(data.size()), &rd, nullptr);
		CloseHandle(h);
		if (rd < data.size())
			data.resize(rd);
		return data;
	}

	bool WriteAll(const std::wstring& path, const std::string& data)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD wr = 0;
		const BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &wr, nullptr);
		CloseHandle(h);
		return ok != FALSE;
	}

	std::string JsonField(const std::string& json, const char* key)
	{
		const JsonView::View v = JsonView::AsView(json);
		std::string s = JsonView::StringAfterKey(v, key);
		if (!s.empty())
			return s;
		const long long n = JsonView::IntAfterKey(v, key);
		if (n > 0)
			return std::to_string(n);
		return {};
	}

	bool ParseManifest(const std::string& json, RemoteManifest* out)
	{
		if (!out)
			return false;
		out->catalog = JsonField(json, "catalog");
		out->icons = JsonField(json, "icons");
		out->cef = JsonField(json, "cef");
		return !out->catalog.empty() || !out->icons.empty() || !out->cef.empty();
	}

	std::string FormatManifest(const RemoteManifest& m)
	{
		std::string out = "{\n";
		bool any = false;
		auto add = [&](const char* key, const std::string& val) {
			if (val.empty())
				return;
			if (any)
				out += ",\n";
			any = true;
			out += "  \"";
			out += key;
			out += "\": \"";
			out += val;
			out += '"';
		};
		add("catalog", m.catalog);
		add("cef", m.cef);
		add("icons", m.icons);
		out += "\n}\n";
		return out;
	}

	void MergeLocalManifest(const char* catalog, const char* icons, const char* cef)
	{
		RemoteManifest m;
		ParseManifest(ReadAll(ManifestPath(), 4096), &m);
		if (catalog && catalog[0])
			m.catalog = catalog;
		if (icons && icons[0])
			m.icons = icons;
		if (cef && cef[0])
			m.cef = cef;
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (!m.catalog.empty())
				gBuild = m.catalog;
			if (!m.icons.empty())
				gIconsHash = m.icons;
		}
		WriteAll(ManifestPath(), FormatManifest(m));
	}

	std::string NormalizeIcon(std::string s)
	{
		while (!s.empty() && (s.back() == '\r' || s.back() == ' '))
			s.pop_back();
		if (s.empty())
			return {};
		if (s.rfind("https://render.guildwars2.com/", 0) == 0)
			return s;
		if (s.find("://") != std::string::npos)
			return {};
		if (s.find('/') == std::string::npos)
			return {};
		return std::string(kRenderPrefix) + s;
	}

	bool LookupLocked(const std::unordered_map<int, std::string>& map, int id, std::string* out)
	{
		auto it = map.find(id);
		if (it == map.end() || it->second.empty())
			return false;
		*out = it->second;
		return true;
	}

	bool LookupKind(char kind,
		const std::unordered_map<char, std::unordered_map<int, std::string>>& root,
		int id, std::string* out)
	{
		auto kit = root.find(kind);
		if (kit == root.end())
			return false;
		return LookupLocked(kit->second, id, out);
	}

	bool ExtraLooksLikeIcon(const std::string& s)
	{
		if (s.find('/') != std::string::npos)
			return true;
		if (s.rfind("https://", 0) == 0)
			return true;
		return false;
	}

	void ParseTsv(const std::string& tsv)
	{
		std::unordered_map<char, std::unordered_map<int, std::string>> names, icons, extra;
		std::string build;
		names['i'].reserve(65536);
		names['s'].reserve(16384);
		size_t i = 0;
		while (i < tsv.size())
		{
			size_t eol = tsv.find('\n', i);
			if (eol == std::string::npos)
				eol = tsv.size();
			std::string line = tsv.substr(i, eol - i);
			i = eol + 1;
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.empty())
				continue;
			if (line[0] == '#')
			{
				if (line.rfind("# build ", 0) == 0)
					build = line.substr(8);
				continue;
			}
			const char kind = line[0];
			if (line.size() < 4 || line[1] != '\t')
				continue;
			if (kind < 'a' || kind > 'z' || kind == 'r')
				continue;
			const size_t tab = line.find('\t', 2);
			if (tab == std::string::npos)
				continue;
			const int id = std::atoi(line.c_str() + 2);
			if (id <= 0)
				continue;
			const size_t tab2 = line.find('\t', tab + 1);
			std::string name;
			std::string fourth;
			if (tab2 == std::string::npos)
				name = line.substr(tab + 1);
			else
			{
				name = line.substr(tab + 1, tab2 - tab - 1);
				fourth = line.substr(tab2 + 1);
				while (!fourth.empty() && (fourth.back() == '\r' || fourth.back() == ' '))
					fourth.pop_back();
			}
			if (name.empty() && fourth.empty())
				continue;
			if (!name.empty())
				names[kind][id] = std::move(name);
			if (fourth.empty())
				continue;
			if (ExtraLooksLikeIcon(fourth))
			{
				std::string icon = NormalizeIcon(std::move(fourth));
				if (!icon.empty())
					icons[kind][id] = std::move(icon);
			}
			else
				extra[kind][id] = std::move(fourth);
		}
		std::lock_guard<std::mutex> lock(gMu);
		gNames.swap(names);
		gIcons.swap(icons);
		gExtra.swap(extra);
		if (!build.empty())
			gBuild = std::move(build);
	}

	void ParseIngs(const std::string& s, std::vector<std::pair<int, int>>& out)
	{
		size_t i = 0;
		while (i < s.size())
		{
			int id = 0;
			int cnt = 0;
			while (i < s.size() && s[i] >= '0' && s[i] <= '9')
				id = id * 10 + (s[i++] - '0');
			if (i >= s.size() || s[i] != ':')
				break;
			++i;
			while (i < s.size() && s[i] >= '0' && s[i] <= '9')
				cnt = cnt * 10 + (s[i++] - '0');
			if (id > 0 && cnt > 0)
				out.emplace_back(id, cnt);
			if (i < s.size() && s[i] == ',')
				++i;
			else
				break;
		}
	}

	void ParseRecipes(const std::string& tsv)
	{
		std::unordered_map<int, Gw2Catalog::Recipe> recs;
		std::unordered_map<int, std::vector<int>> byOut;
		recs.reserve(16384);
		size_t i = 0;
		while (i < tsv.size())
		{
			size_t eol = tsv.find('\n', i);
			if (eol == std::string::npos)
				eol = tsv.size();
			std::string line = tsv.substr(i, eol - i);
			i = eol + 1;
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			if (line.empty() || line[0] == '#')
				continue;
			if (line.size() < 4 || line[0] != 'r' || line[1] != '\t')
				continue;
			int fields[4] = {};
			size_t pos = 2;
			bool ok = true;
			for (int f = 0; f < 4; ++f)
			{
				if (pos >= line.size())
				{
					ok = false;
					break;
				}
				fields[f] = std::atoi(line.c_str() + static_cast<int>(pos));
				const size_t tab = line.find('\t', pos);
				if (tab == std::string::npos)
				{
					ok = false;
					break;
				}
				pos = tab + 1;
			}
			if (!ok || fields[0] <= 0 || fields[1] <= 0)
				continue;
			Gw2Catalog::Recipe rec;
			rec.recipeId = fields[0];
			rec.outputId = fields[1];
			rec.outputCount = fields[2] > 0 ? fields[2] : 1;
			rec.minRating = fields[3] < 0 ? 0 : fields[3];
			const size_t tab = line.find('\t', pos);
			if (tab == std::string::npos)
				rec.disciplines = line.substr(pos);
			else
			{
				rec.disciplines = line.substr(pos, tab - pos);
				ParseIngs(line.substr(tab + 1), rec.ings);
			}
			byOut[rec.outputId].push_back(rec.recipeId);
			recs[rec.recipeId] = std::move(rec);
		}
		std::lock_guard<std::mutex> lock(gMu);
		gRecipes.swap(recs);
		gByOutput.swap(byOut);
		gRecipesOnDisk = !gRecipes.empty();
	}

	void LoadDisk()
	{
		if (gDiskLoaded)
			return;
		gDiskLoaded = true;
		RemoteManifest man;
		ParseManifest(ReadAll(ManifestPath(), 4096), &man);
		const std::string tsv = ReadAll(TsvPath(), 24u * 1024u * 1024u);
		if (!tsv.empty())
			ParseTsv(tsv);
		const std::string rec = ReadAll(RecipesPath(), 16u * 1024u * 1024u);
		if (!rec.empty())
			ParseRecipes(rec);
		if ((tsv.empty() || rec.empty()) && TryApplyLocalIgh())
			return;
		TryOpenLocalIcons();
		if (!man.catalog.empty() || !man.icons.empty())
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (!man.catalog.empty())
				gBuild = std::move(man.catalog);
			if (!man.icons.empty())
				gIconsHash = std::move(man.icons);
		}
	}

	bool DiscMatch(const std::string& discs, const char* prefer)
	{
		if (!prefer || !prefer[0] || discs.empty())
			return false;
		size_t i = 0;
		while (i <= discs.size())
		{
			size_t bar = discs.find('|', i);
			if (bar == std::string::npos)
				bar = discs.size();
			if (discs.compare(i, bar - i, prefer) == 0)
				return true;
			if (bar == discs.size())
				break;
			i = bar + 1;
		}
		return false;
	}
}
