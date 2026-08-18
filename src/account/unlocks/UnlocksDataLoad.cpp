#include "UnlocksDataInternal.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "Gw2Catalog.h"
#include "Gw2Http.h"
#include "JsonView.h"

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

namespace UnlocksDetail
{
	bool WriteUtf8File(const std::wstring& path, const std::string& data)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
		CloseHandle(h);
		return ok && written == data.size();
	}

	std::string ReadUtf8File(const std::wstring& path)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return {};
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 16 * 1024 * 1024)
		{
			CloseHandle(h);
			return {};
		}
		std::string out(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD read = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
		CloseHandle(h);
		if (!ok || read != out.size())
			return {};
		return out;
	}

	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from)
	{
		return JsonView::StringAfterKey(json, key, from);
	}

	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from)
	{
		return JsonView::IntAfterKey(json, key, from);
	}

	void ParseIdArray(const std::string& body, std::unordered_set<int>& out)
	{
		JsonView::ParseIdArray(body, out);
	}

	/* Finishers arrive as [{"id":N,"permanent":true},...] - collect id fields. */
	void ParseFinisherIds(const std::string& body, std::unordered_set<int>& out)
	{
		out.clear();
		size_t p = 0;
		while (p < body.size())
		{
			const size_t k = body.find("\"id\"", p);
			if (k == std::string::npos)
				break;
			const long long id = JsonIntAfterKey(body, "id", k);
			if (id > 0)
				out.insert(static_cast<int>(id));
			p = k + 4;
		}
		if (out.empty())
			ParseIdArray(body, out);
	}

	static int ClampByte(int n)
	{
		if (n < 0)
			return 0;
		if (n > 255)
			return 255;
		return n;
	}

	static bool ParseRgbArray(const std::string& json, size_t from, size_t limit, unsigned* packed)
	{
		if (!packed || from >= limit || from == std::string::npos)
			return false;
		const size_t br = json.find('[', from);
		if (br == std::string::npos || br >= limit)
			return false;
		int rv = 0, gv = 0, bv = 0;
		if (std::sscanf(json.c_str() + br, "[%d,%d,%d]", &rv, &gv, &bv) != 3)
			return false;
		*packed = (static_cast<unsigned>(ClampByte(rv)) << 16) |
			(static_cast<unsigned>(ClampByte(gv)) << 8) |
			static_cast<unsigned>(ClampByte(bv));
		return true;
	}

	static size_t FindIn(const std::string& s, const char* needle, size_t from, size_t limit)
	{
		if (!needle || from >= limit)
			return std::string::npos;
		const size_t p = s.find(needle, from);
		if (p == std::string::npos || p >= limit)
			return std::string::npos;
		return p;
	}

	/* /v2/colors: cloth/leather/metal.rgb, not item icons. base_rgb is often a shared red. */
	static bool ParseColorSwatch(const std::string& body, size_t brace, size_t end, unsigned* packed)
	{
		if (!packed)
			return false;
		const size_t cloth = FindIn(body, "\"cloth\"", brace, end);
		const size_t leather = FindIn(body, "\"leather\"", brace, end);
		const size_t metal = FindIn(body, "\"metal\"", brace, end);
		const size_t clothRgb = (cloth != std::string::npos)
			? FindIn(body, "\"rgb\"", cloth, end) : std::string::npos;
		const size_t leatherRgb = (leather != std::string::npos)
			? FindIn(body, "\"rgb\"", leather, end) : std::string::npos;
		const size_t metalRgb = (metal != std::string::npos)
			? FindIn(body, "\"rgb\"", metal, end) : std::string::npos;
		const size_t baseRgb = FindIn(body, "\"base_rgb\"", brace, end);
		return ParseRgbArray(body, clothRgb, end, packed) ||
			ParseRgbArray(body, leatherRgb, end, packed) ||
			ParseRgbArray(body, metalRgb, end, packed) ||
			ParseRgbArray(body, baseRgb, end, packed);
	}

	static bool ParseRgbTriple(const std::string& s, unsigned* packed)
	{
		if (!packed || s.empty())
			return false;
		int rv = 0, gv = 0, bv = 0;
		if (std::sscanf(s.c_str(), "%d,%d,%d", &rv, &gv, &bv) == 3)
		{
			*packed = (static_cast<unsigned>(ClampByte(rv)) << 16) |
				(static_cast<unsigned>(ClampByte(gv)) << 8) |
				static_cast<unsigned>(ClampByte(bv));
			return true;
		}
		const char* p = s.c_str();
		if (*p == '#')
			++p;
		unsigned u = 0;
		if (std::strlen(p) == 6 && std::sscanf(p, "%6x", &u) == 1)
		{
			*packed = u & 0xFFFFFFu;
			return true;
		}
		return false;
	}

	static std::string NormalizeUnlockIcon(std::string s)
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
		return std::string("https://render.guildwars2.com/file/") + s;
	}

	void ParseNameObjects(const std::string& body, std::unordered_map<int, std::string>& names,
		std::unordered_map<int, unsigned>* rgb, std::unordered_map<int, std::string>* icons)
	{
		size_t p = 0;
		while (p < body.size())
		{
			const size_t brace = body.find('{', p);
			if (brace == std::string::npos)
				break;
			int depth = 0;
			bool inStr = false, esc = false;
			size_t end = brace;
			for (; end < body.size(); ++end)
			{
				const char c = body[end];
				if (inStr)
				{
					if (esc) esc = false;
					else if (c == '\\') esc = true;
					else if (c == '"') inStr = false;
					continue;
				}
				if (c == '"') { inStr = true; continue; }
				if (c == '{') ++depth;
				else if (c == '}')
				{
					--depth;
					if (depth == 0)
					{
						++end;
						break;
					}
				}
			}
			const long long id = JsonIntAfterKey(body, "id", brace);
			std::string name = JsonStringAfterKey(body, "name", brace);
			if (id > 0)
			{
				if (!name.empty())
					names[static_cast<int>(id)] = std::move(name);
				if (rgb)
				{
					unsigned packed = 0;
					if (ParseColorSwatch(body, brace, end, &packed))
						(*rgb)[static_cast<int>(id)] = packed;
				}
				if (icons)
				{
					const std::string obj = body.substr(brace, end > brace ? end - brace : 0);
					std::string icon = NormalizeUnlockIcon(JsonStringAfterKey(obj, "icon", 0));
					if (!icon.empty())
						(*icons)[static_cast<int>(id)] = std::move(icon);
				}
			}
			p = end;
		}
	}

	bool LoadCache(UnlocksData::Kind k, std::unordered_set<int>& ids,
		std::unordered_map<int, std::string>& names,
		std::unordered_map<int, unsigned>* rgb,
		std::unordered_map<int, std::string>* icons)
	{
		const std::wstring path = CachePathW(k);
		WIN32_FILE_ATTRIBUTE_DATA fad{};
		if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad))
			return false;
		FILETIME nowFt{};
		GetSystemTimeAsFileTime(&nowFt);
		ULARGE_INTEGER now{}, then{};
		now.LowPart = nowFt.dwLowDateTime;
		now.HighPart = nowFt.dwHighDateTime;
		then.LowPart = fad.ftLastWriteTime.dwLowDateTime;
		then.HighPart = fad.ftLastWriteTime.dwHighDateTime;
		if ((now.QuadPart - then.QuadPart) / 10000ULL > kCacheTtlMs)
			return false;

		const std::string raw = ReadUtf8File(path);
		if (raw.rfind("#gw2igh-unlocks v1", 0) != 0)
			return false;
		ids.clear();
		names.clear();
		if (rgb)
			rgb->clear();
		if (icons)
			icons->clear();
		size_t p = raw.find('\n');
		if (p == std::string::npos)
			return false;
		++p;
		while (p < raw.size())
		{
			size_t end = raw.find('\n', p);
			if (end == std::string::npos)
				end = raw.size();
			std::string line = raw.substr(p, end - p);
			p = end + 1;
			if (line.empty() || line[0] == '#')
				continue;
			const size_t tab = line.find('\t');
			int id = 0;
			if (!JsonView::ParseInt32(JsonView::AsView(line), 0, &id, nullptr) || id <= 0)
				continue;
			ids.insert(id);
			if (tab == std::string::npos || tab + 1 >= line.size())
				continue;
			const size_t tab2 = line.find('\t', tab + 1);
			if (tab2 == std::string::npos)
				names[id] = line.substr(tab + 1);
			else
			{
				names[id] = line.substr(tab + 1, tab2 - tab - 1);
				const std::string extra = line.substr(tab2 + 1);
				if (rgb)
				{
					unsigned packed = 0;
					if (ParseRgbTriple(extra, &packed))
						(*rgb)[id] = packed;
				}
				if (icons)
				{
					std::string icon = NormalizeUnlockIcon(extra);
					if (!icon.empty())
						(*icons)[id] = std::move(icon);
				}
			}
		}
		return !ids.empty();
	}

	void SaveCache(UnlocksData::Kind k, const std::unordered_set<int>& ids,
		const std::unordered_map<int, std::string>& names,
		const std::unordered_map<int, unsigned>* rgb,
		const std::unordered_map<int, std::string>* icons)
	{
		std::string out = "#gw2igh-unlocks v1\n";
		std::vector<int> sorted(ids.begin(), ids.end());
		std::sort(sorted.begin(), sorted.end());
		for (int id : sorted)
		{
			out += std::to_string(id);
			out += '\t';
			auto it = names.find(id);
			if (it != names.end())
			{
				for (char c : it->second)
					out.push_back((c == '\t' || c == '\n') ? ' ' : c);
			}
			if (rgb)
			{
				auto rit = rgb->find(id);
				if (rit != rgb->end())
				{
					char buf[24];
					std::snprintf(buf, sizeof(buf), "\t%u,%u,%u",
						(rit->second >> 16) & 255u,
						(rit->second >> 8) & 255u,
						rit->second & 255u);
					out += buf;
				}
			}
			else if (icons)
			{
				auto iit = icons->find(id);
				if (iit != icons->end() && !iit->second.empty())
				{
					out += '\t';
					for (char c : iit->second)
						out.push_back((c == '\t' || c == '\n') ? ' ' : c);
				}
			}
			out += '\n';
		}
		CreateDirectoryW(AddonPaths::DataDir().c_str(), nullptr);
		WriteUtf8File(CachePathW(k), out);
	}

	char CatalogKind(UnlocksData::Kind k)
	{
		switch (k)
		{
		case UnlocksData::Kind::Skins: return 's';
		case UnlocksData::Kind::Dyes: return 'd';
		case UnlocksData::Kind::Minis: return 'n';
		case UnlocksData::Kind::Finishers: return 'f';
		case UnlocksData::Kind::Outfits: return 'o';
		case UnlocksData::Kind::Gliders: return 'g';
		case UnlocksData::Kind::MailCarriers: return 'u';
		case UnlocksData::Kind::Novelties: return 'v';
		case UnlocksData::Kind::Titles: return 't';
		default: return 0;
		}
	}

	void ResolveDyeRgb(const std::unordered_set<int>& ids,
		std::unordered_map<int, std::string>& names,
		std::unordered_map<int, unsigned>& rgb)
	{
		std::vector<int> missing;
		missing.reserve(ids.size());
		for (int id : ids)
		{
			if (rgb.find(id) == rgb.end())
			{
				int r = 0, g = 0, b = 0;
				if (Gw2Catalog::DyeRgb(id, &r, &g, &b))
					rgb[id] = (static_cast<unsigned>(r) << 16) |
						(static_cast<unsigned>(g) << 8) |
						static_cast<unsigned>(b);
				else
					missing.push_back(id);
			}
			if (names.find(id) != names.end())
				continue;
			std::string cat;
			if (Gw2Catalog::Name('d', id, &cat))
				names[id] = std::move(cat);
		}
		std::sort(missing.begin(), missing.end());
		for (size_t i = 0; i < missing.size(); )
		{
			std::string path = "/v2/colors?ids=";
			const size_t end = (std::min)(i + static_cast<size_t>(kNameBatch), missing.size());
			for (size_t j = i; j < end; ++j)
			{
				if (j > i)
					path += ",";
				path += std::to_string(missing[j]);
			}
			auto r = Gw2Http::Api(path.c_str(), nullptr, kHttpTimeoutMs);
			if (r.ok)
				ParseNameObjects(r.body, names, &rgb);
			i = end;
		}
	}

	void ResolveUnlockIcons(UnlocksData::Kind k, const std::unordered_set<int>& ids,
		std::unordered_map<int, std::string>& names,
		std::unordered_map<int, std::string>& icons)
	{
		if (k == UnlocksData::Kind::Titles || k == UnlocksData::Kind::Dyes)
			return;
		const char ck = CatalogKind(k);
		std::vector<int> missing;
		missing.reserve(ids.size());
		for (int id : ids)
		{
			if (icons.find(id) != icons.end())
				continue;
			std::string icon;
			if (ck && Gw2Catalog::Icon(ck, id, &icon) && !icon.empty())
				icons[id] = std::move(icon);
			else
				missing.push_back(id);
		}
		std::sort(missing.begin(), missing.end());
		for (size_t i = 0; i < missing.size(); )
		{
			std::string path = kPublicPaths[KindIndex(k)];
			path += "?ids=";
			const size_t end = (std::min)(i + static_cast<size_t>(kNameBatch), missing.size());
			for (size_t j = i; j < end; ++j)
			{
				if (j > i)
					path += ",";
				path += std::to_string(missing[j]);
			}
			auto r = Gw2Http::Api(path.c_str(), nullptr, kHttpTimeoutMs);
			if (r.ok)
				ParseNameObjects(r.body, names, nullptr, &icons);
			i = end;
		}
	}

	void ResolveNames(UnlocksData::Kind k, const std::unordered_set<int>& ids,
		std::unordered_map<int, std::string>& names, const char* key)
	{
		std::vector<int> missing;
		missing.reserve(ids.size());
		for (int id : ids)
		{
			if (names.find(id) != names.end())
				continue;
			std::string cat;
			const char ck = CatalogKind(k);
			if (ck && Gw2Catalog::Name(ck, id, &cat))
				names[id] = std::move(cat);
			else
				missing.push_back(id);
		}
		std::sort(missing.begin(), missing.end());
		for (size_t i = 0; i < missing.size(); )
		{
			std::string path = kPublicPaths[KindIndex(k)];
			path += "?ids=";
			const size_t end = (std::min)(i + static_cast<size_t>(kNameBatch), missing.size());
			for (size_t j = i; j < end; ++j)
			{
				if (j > i)
					path += ",";
				path += std::to_string(missing[j]);
			}
			auto r = Gw2Http::Api(path.c_str(), nullptr, kHttpTimeoutMs);
			if (r.ok)
				ParseNameObjects(r.body, names, nullptr, nullptr);
			i = end;
		}
		(void)key;
	}

	DWORD WINAPI LoadProc(void* p)
	{
		LoadArg* arg = static_cast<LoadArg*>(p);
		const UnlocksData::Kind kind = arg->kind;
		const bool force = arg->force;
		delete arg;

		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		KindState& st = gKinds[KindIndex(kind)];
		std::unordered_set<int> ids;
		std::unordered_map<int, std::string> names;
		std::unordered_map<int, unsigned> rgb;
		std::unordered_map<int, std::string> icons;
		std::string status;

		const bool isDyes = kind == UnlocksData::Kind::Dyes;
		const bool wantsIcons = !isDyes && kind != UnlocksData::Kind::Titles;
		if (!force && LoadCache(kind, ids, names,
			isDyes ? &rgb : nullptr, wantsIcons ? &icons : nullptr))
		{
			status = "Loaded " + std::to_string(ids.size()) + " (cache).";
		}
		else if (!G::Gw2ApiKey[0])
		{
			status = "API key required (unlocks scope).";
		}
		else
		{
			status = "Downloading...";
			{
				std::lock_guard<std::mutex> lock(gMu);
				st.pendingStatus = status;
			}
			auto r = Gw2Http::Api(kAccountPaths[KindIndex(kind)], G::Gw2ApiKey, kHttpTimeoutMs);
			if (!r.ok)
			{
				status = r.status == 403
					? "Missing unlocks scope on API key."
					: ("Download failed: " + r.error);
			}
			else
			{
				if (kind == UnlocksData::Kind::Finishers)
					ParseFinisherIds(r.body, ids);
				else
					ParseIdArray(r.body, ids);
				ResolveNames(kind, ids, names, G::Gw2ApiKey);
				if (isDyes)
					ResolveDyeRgb(ids, names, rgb);
				else if (wantsIcons)
					ResolveUnlockIcons(kind, ids, names, icons);
				SaveCache(kind, ids, names, isDyes ? &rgb : nullptr,
					wantsIcons ? &icons : nullptr);
				status = "Indexed " + std::to_string(ids.size()) + " " +
					kLabels[KindIndex(kind)] + ".";
			}
		}

		if (isDyes && !ids.empty() && rgb.size() < ids.size())
		{
			ResolveDyeRgb(ids, names, rgb);
			if (!rgb.empty())
				SaveCache(kind, ids, names, &rgb, nullptr);
		}
		else if (wantsIcons && !ids.empty() && icons.size() < ids.size())
		{
			ResolveUnlockIcons(kind, ids, names, icons);
			if (!icons.empty())
				SaveCache(kind, ids, names, nullptr, &icons);
		}

		{
			std::lock_guard<std::mutex> lock(gMu);
			st.pendingIds = std::move(ids);
			st.pendingNames = std::move(names);
			st.pendingRgb = std::move(rgb);
			st.pendingIcons = std::move(icons);
			st.pendingStatus = status;
			st.pending = true;
			st.busy = false;
		}
		return 0;
	}
} // namespace UnlocksDetail
