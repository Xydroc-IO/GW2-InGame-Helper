#include "CraftingData.h"

#include "CraftingShared.h"

#include "Globals.h"
#include "Gw2Http.h"

#include "AddonPaths.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace CraftingDetail
{
	size_t JsonObjectEnd(const std::string& json, size_t openBrace)
	{
		if (openBrace >= json.size() || json[openBrace] != '{')
			return std::string::npos;
		int depth = 0;
		bool inStr = false, esc = false;
		for (size_t i = openBrace; i < json.size(); ++i)
		{
			char c = json[i];
			if (inStr)
			{
				if (esc) esc = false;
				else if (c == '\\') esc = true;
				else if (c == '"') inStr = false;
				continue;
			}
			if (c == '"') inStr = true;
			else if (c == '{') ++depth;
			else if (c == '}')
			{
				--depth;
				if (depth == 0) return i;
			}
		}
		return std::string::npos;
	}

	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos) return {};
		k = json.find(':', k + pat.size());
		if (k == std::string::npos) return {};
		++k;
		while (k < json.size() && (json[k] == ' ' || json[k] == '\t')) ++k;
		if (k >= json.size() || json[k] != '"') return {};
		++k;
		std::string out;
		while (k < json.size())
		{
			char c = json[k++];
			if (c == '\\' && k < json.size())
			{
				char e = json[k++];
				if (e == 'n') out.push_back('\n');
				else if (e == 't') out.push_back('\t');
				else if (e == 'u' && k + 3 < json.size()) k += 4;
				else out.push_back(e);
				continue;
			}
			if (c == '"') break;
			out.push_back(c);
		}
		return out;
	}

	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos) return -1;
		k = json.find(':', k + pat.size());
		if (k == std::string::npos) return -1;
		++k;
		while (k < json.size() && (json[k] == ' ' || json[k] == '\t')) ++k;
		bool neg = false;
		if (k < json.size() && json[k] == '-') { neg = true; ++k; }
		long long v = 0;
		bool any = false;
		while (k < json.size() && json[k] >= '0' && json[k] <= '9')
		{
			any = true;
			v = v * 10 + (json[k] - '0');
			++k;
		}
		if (!any) return -1;
		return neg ? -v : v;
	}

	std::string FormatCoins(long long copper)
	{
		if (copper < 0) copper = 0;
		const long long g = copper / 10000;
		const long long s = (copper % 10000) / 100;
		const long long c = copper % 100;
		char buf[64];
		if (g > 0)
			std::snprintf(buf, sizeof(buf), "%lldg %02llds %02lldc", g, s, c);
		else if (s > 0)
			std::snprintf(buf, sizeof(buf), "%llds %02lldc", s, c);
		else
			std::snprintf(buf, sizeof(buf), "%lldc", c);
		return buf;
	}

	std::string UrlEncode(const char* s)
	{
		std::string o;
		static const char* hex = "0123456789ABCDEF";
		for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p)
		{
			unsigned char c = *p;
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
				o.push_back(static_cast<char>(c));
			else if (c == ' ')
				o.push_back('+');
			else
			{
				o.push_back('%');
				o.push_back(hex[c >> 4]);
				o.push_back(hex[c & 15]);
			}
		}
		return o;
	}

	int ParseItemId(const char* text)
	{
		if (!text || !text[0]) return 0;
		const char* a = std::strstr(text, "[&");
		if (a)
		{
			a += 2;
			const char* b = std::strchr(a, ']');
			if (b && b > a)
			{
				static const char kB64[] =
					"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
				int buf = 0, bits = 0;
				unsigned char out[16]{};
				size_t n = 0;
				for (const char* p = a; p < b && n < sizeof(out); ++p)
				{
					if (*p == '=' || *p == ' ') break;
					const char* q = std::strchr(kB64, *p);
					if (!q) continue;
					buf = (buf << 6) | static_cast<int>(q - kB64);
					bits += 6;
					if (bits >= 8)
					{
						bits -= 8;
						out[n++] = static_cast<unsigned char>((buf >> bits) & 0xFF);
					}
				}
				if (n >= 5 && out[0] == 0x02)
				{
					const int id = out[2] | (out[3] << 8) | (out[4] << 16);
					if (id > 0) return id;
				}
			}
		}
		int id = 0;
		bool onlyDigits = true;
		for (const char* p = text; *p; ++p)
		{
			if (*p == ' ' || *p == '\t') continue;
			if (*p >= '0' && *p <= '9')
				id = id * 10 + (*p - '0');
			else { onlyDigits = false; break; }
		}
		return (onlyDigits && id > 0) ? id : 0;
	}
	std::string ItemName(int id)
	{
		char path[64];
		std::snprintf(path, sizeof(path), "/v2/items/%d", id);
		auto r = Gw2Http::Api(path, nullptr, kHttpTimeoutMs);
		if (!r.ok || r.body.empty() || r.body[0] != '{') return {};
		return JsonStringAfterKey(r.body, "name", 0);
	}

	void FetchNames(std::unordered_map<int, std::string>& names, const std::vector<int>& ids)
	{
		std::vector<int> need;
		for (int id : ids)
			if (names.find(id) == names.end()) need.push_back(id);
		for (size_t off = 0; off < need.size(); off += 200)
		{
			const size_t n = (std::min)(need.size() - off, size_t{200});
			std::string path = "/v2/items?ids=";
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
				long long id = JsonIntAfterKey(r.body, "id", brace);
				std::string name = JsonStringAfterKey(r.body, "name", brace);
				if (id > 0 && !name.empty())
					names[static_cast<int>(id)] = name;
				p = end + 1;
			}
		}
	}

	/* First candidate that exists on the official items API (one bulk request). */
	int FirstValidItemId(const std::vector<int>& candidates, std::string* nameOut)
	{
		if (candidates.empty()) return 0;
		std::unordered_map<int, std::string> names;
		FetchNames(names, candidates);
		for (int id : candidates)
		{
			auto it = names.find(id);
			if (it != names.end() && !it->second.empty())
			{
				if (nameOut) *nameOut = it->second;
				return id;
			}
		}
		return 0;
	}

	/* Instant-buy unit price = lowest sell listing (not buy-order).
	   Optional buys map = highest buy-order unit (instant sell). */
	void FetchPrices(std::unordered_map<int, long long>& sells, const std::vector<int>& ids,
		std::unordered_map<int, long long>* buys)
	{
		for (size_t off = 0; off < ids.size(); off += 200)
		{
			const size_t n = (std::min)(ids.size() - off, size_t{200});
			std::string path = "/v2/commerce/prices?ids=";
			for (size_t i = 0; i < n; ++i)
			{
				if (i) path += ',';
				path += std::to_string(ids[off + i]);
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
				long long id = JsonIntAfterKey(r.body, "id", brace);
				size_t sellsKey = r.body.find("\"sells\"", brace);
				long long sellUnit = -1;
				if (sellsKey != std::string::npos && sellsKey < end)
					sellUnit = JsonIntAfterKey(r.body, "unit_price", sellsKey);
				if (id > 0 && sellUnit >= 0)
					sells[static_cast<int>(id)] = sellUnit;
				if (buys)
				{
					size_t buysKey = r.body.find("\"buys\"", brace);
					long long buyUnit = -1;
					if (buysKey != std::string::npos && buysKey < end)
						buyUnit = JsonIntAfterKey(r.body, "unit_price", buysKey);
					if (id > 0 && buyUnit >= 0)
						(*buys)[static_cast<int>(id)] = buyUnit;
				}
				p = end + 1;
			}
		}
	}

	void ParseIntArray(const std::string& body, std::vector<int>& out)
	{
		size_t i = 0;
		while (i < body.size() && out.size() < 20000)
		{
			while (i < body.size() && (body[i] < '0' || body[i] > '9') && body[i] != '-')
				++i;
			if (i >= body.size()) break;
			long long v = 0;
			bool neg = false;
			if (body[i] == '-') { neg = true; ++i; }
			if (i >= body.size() || body[i] < '0' || body[i] > '9') break;
			while (i < body.size() && body[i] >= '0' && body[i] <= '9')
				v = v * 10 + (body[i++] - '0');
			if (neg) v = -v;
			if (v > 0 && v < 2000000000)
				out.push_back(static_cast<int>(v));
		}
	}

	void ParseQuotedStringArray(const std::string& body, std::vector<std::string>& out)
	{
		size_t i = 0;
		while (i < body.size() && out.size() < 256)
		{
			while (i < body.size() && body[i] != '"') ++i;
			if (i >= body.size()) break;
			++i;
			std::string val;
			while (i < body.size() && body[i] != '"')
			{
				if (body[i] == '\\' && i + 1 < body.size())
				{
					val.push_back(body[i + 1]);
					i += 2;
					continue;
				}
				val.push_back(body[i++]);
			}
			if (i < body.size()) ++i;
			if (!val.empty()) out.push_back(val);
		}
	}

	std::string EncodeCharPath(const std::string& name)
	{
		std::string o;
		for (unsigned char c : name)
		{
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				(c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
				o.push_back(static_cast<char>(c));
			else if (c == ' ')
				o += "%20";
			else
			{
				char buf[8];
				std::snprintf(buf, sizeof(buf), "%%%02X", c);
				o += buf;
			}
		}
		return o;
	}

	bool WriteUtf8File(const std::wstring& path, const std::string& body)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, body.data(), static_cast<DWORD>(body.size()), &written, nullptr);
		CloseHandle(h);
		return ok != 0;
	}

	bool ReadUtf8File(const std::wstring& path, std::string& out)
	{
		out.clear();
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 8 * 1024 * 1024)
		{
			CloseHandle(h);
			return false;
		}
		out.resize(static_cast<size_t>(sz.QuadPart));
		DWORD got = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &got, nullptr);
		CloseHandle(h);
		if (!ok) { out.clear(); return false; }
		out.resize(got);
		return true;
	}

	std::wstring ConfigFile(const wchar_t* leaf)
	{
		std::wstring dir = AddonPaths::ConfigDir();
		if (dir.empty()) dir = AddonPaths::DataDir();
		if (dir.empty() || !leaf) return {};
		if (dir.back() != L'\\' && dir.back() != L'/') dir.push_back(L'\\');
		dir += leaf;
		return dir;
	}
} // namespace CraftingDetail
