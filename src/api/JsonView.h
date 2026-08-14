#pragma once

/* Bounds-checked GW2 JSON scrapers (C++17).
   Prefer these over atoi/atoll/strtod on raw c_str()+offset.
   View = string_view; Bytes = explicit (ptr,len) stand-in for span<const char>. */

#include <climits>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_set>

namespace JsonView
{
	using View = std::string_view;

	/* Contiguous bytes with explicit length (C++17 stand-in for std::span<const char>). */
	struct Bytes
	{
		const char* data = nullptr;
		size_t size = 0;

		constexpr Bytes() = default;
		constexpr Bytes(const char* p, size_t n) : data(p), size(n) {}
		Bytes(View v) : data(v.data()), size(v.size()) {}
		Bytes(const std::string& s) : data(s.data()), size(s.size()) {}

		constexpr const char* begin() const { return data; }
		constexpr const char* end() const { return data + size; }
		constexpr bool empty() const { return size == 0; }
		constexpr char operator[](size_t i) const { return data[i]; }
		View view() const { return View(data, size); }
	};

	inline View AsView(const std::string& s) { return View(s.data(), s.size()); }
	inline View AsView(Bytes b) { return b.view(); }
	inline View AsView(View v) { return v; }

	inline size_t SkipWs(View s, size_t i)
	{
		while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n'))
			++i;
		return i;
	}

	/* Find `"key"` at or after `from`. Returns npos if missing. */
	inline size_t FindQuotedKey(View json, View key, size_t from = 0)
	{
		if (key.empty() || json.size() < key.size() + 2)
			return View::npos;
		const size_t last = json.size() - key.size() - 2;
		for (size_t i = from; i <= last; ++i)
		{
			if (json[i] != '"')
				continue;
			if (json.compare(i + 1, key.size(), key) != 0)
				continue;
			if (json[i + 1 + key.size()] != '"')
				continue;
			return i;
		}
		return View::npos;
	}

	/* After `"key"`, skip to the first non-ws char of the value. */
	inline size_t ValueStartAfterKey(View json, View key, size_t from = 0)
	{
		size_t k = FindQuotedKey(json, key, from);
		if (k == View::npos)
			return View::npos;
		k += key.size() + 2; /* past closing quote of key */
		k = json.find(':', k);
		if (k == View::npos)
			return View::npos;
		return SkipWs(json, k + 1);
	}

	inline bool ParseInt64(View s, size_t from, long long* out, size_t* after = nullptr)
	{
		if (!out)
			return false;
		size_t k = SkipWs(s, from);
		bool neg = false;
		if (k < s.size() && s[k] == '-')
		{
			neg = true;
			++k;
		}
		long long v = 0;
		bool any = false;
		while (k < s.size() && s[k] >= '0' && s[k] <= '9')
		{
			const int digit = s[k] - '0';
			if (v > (LLONG_MAX - digit) / 10)
			{
				if (after) *after = k;
				return false;
			}
			any = true;
			v = v * 10 + digit;
			++k;
		}
		if (!any)
			return false;
		*out = neg ? -v : v;
		if (after) *after = k;
		return true;
	}

	inline bool ParseInt32(View s, size_t from, int* out, size_t* after = nullptr)
	{
		long long v = 0;
		if (!ParseInt64(s, from, &v, after))
			return false;
		if (v < INT_MIN || v > INT_MAX)
			return false;
		*out = static_cast<int>(v);
		return true;
	}

	/* Copy a float token into a small buffer then strtof — never reads past `s`. */
	inline bool ParseFloat(View s, size_t from, float* out, size_t* after = nullptr)
	{
		if (!out)
			return false;
		size_t k = SkipWs(s, from);
		const size_t start = k;
		if (k < s.size() && (s[k] == '+' || s[k] == '-'))
			++k;
		bool any = false;
		while (k < s.size() && ((s[k] >= '0' && s[k] <= '9') || s[k] == '.' ||
			s[k] == 'e' || s[k] == 'E' || s[k] == '+' || s[k] == '-'))
		{
			any = true;
			++k;
		}
		if (!any || k == start)
			return false;
		char buf[64];
		size_t n = k - start;
		if (n >= sizeof(buf))
			n = sizeof(buf) - 1;
		std::memcpy(buf, s.data() + start, n);
		buf[n] = '\0';
		char* end = nullptr;
		const float v = std::strtof(buf, &end);
		if (!end || end == buf)
			return false;
		*out = v;
		if (after)
			*after = start + static_cast<size_t>(end - buf);
		return true;
	}

	inline std::string ReadQuoted(View s, size_t openQuote, size_t* after = nullptr)
	{
		if (openQuote >= s.size() || s[openQuote] != '"')
		{
			if (after) *after = openQuote;
			return {};
		}
		size_t a = openQuote + 1;
		std::string val;
		while (a < s.size())
		{
			const char c = s[a++];
			if (c == '\\' && a < s.size())
			{
				const char e = s[a++];
				if (e == 'n') val.push_back('\n');
				else if (e == 't') val.push_back('\t');
				else if (e == '"' || e == '\\' || e == '/') val.push_back(e);
				else if (e == 'u' && a + 3 < s.size())
				{
					a += 4;
					val.push_back('?');
				}
				else
					val.push_back(e);
				continue;
			}
			if (c == '"')
				break;
			val.push_back(c);
		}
		if (after) *after = a;
		return val;
	}

	inline std::string StringAfterKey(View json, View key, size_t from = 0)
	{
		const size_t k = ValueStartAfterKey(json, key, from);
		if (k == View::npos || k >= json.size() || json[k] != '"')
			return {};
		return ReadQuoted(json, k, nullptr);
	}

	/* Missing / non-numeric → -1 (matches historical scrapers). */
	inline long long IntAfterKey(View json, View key, size_t from = 0)
	{
		const size_t k = ValueStartAfterKey(json, key, from);
		if (k == View::npos)
			return -1;
		long long v = 0;
		if (!ParseInt64(json, k, &v, nullptr))
			return -1;
		return v;
	}

	inline bool BoolAfterKey(View json, View key, size_t from = 0)
	{
		const size_t k = ValueStartAfterKey(json, key, from);
		if (k == View::npos)
			return false;
		return k + 4 <= json.size() && json.compare(k, 4, "true") == 0;
	}

	inline size_t ObjectEnd(View json, size_t openBrace)
	{
		if (openBrace >= json.size() || json[openBrace] != '{')
			return View::npos;
		int depth = 0;
		bool inStr = false;
		bool esc = false;
		for (size_t i = openBrace; i < json.size(); ++i)
		{
			const char c = json[i];
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
				if (depth == 0)
					return i;
			}
		}
		return View::npos;
	}

	inline size_t ArrayEnd(View json, size_t openBracket)
	{
		if (openBracket >= json.size() || json[openBracket] != '[')
			return View::npos;
		int depth = 0;
		bool inStr = false;
		bool esc = false;
		for (size_t i = openBracket; i < json.size(); ++i)
		{
			const char c = json[i];
			if (inStr)
			{
				if (esc) esc = false;
				else if (c == '\\') esc = true;
				else if (c == '"') inStr = false;
				continue;
			}
			if (c == '"') inStr = true;
			else if (c == '[') ++depth;
			else if (c == ']')
			{
				--depth;
				if (depth == 0)
					return i;
			}
		}
		return View::npos;
	}

	inline bool CoordAfterKey(View json, View key, size_t from, float* outX, float* outY)
	{
		if (!outX || !outY)
			return false;
		size_t k = ValueStartAfterKey(json, key, from);
		if (k == View::npos || k >= json.size() || json[k] != '[')
			return false;
		++k;
		float x = 0.f, y = 0.f;
		size_t after = 0;
		if (!ParseFloat(json, k, &x, &after))
			return false;
		k = after;
		while (k < json.size() && (json[k] == ' ' || json[k] == '\t' || json[k] == ','))
			++k;
		if (!ParseFloat(json, k, &y, nullptr))
			return false;
		*outX = x;
		*outY = y;
		return true;
	}

	/* Digit runs only — used for bare `[1,2,3]` id arrays. */
	inline void ParseIdArray(View body, std::unordered_set<int>& out)
	{
		out.clear();
		for (size_t i = 0; i < body.size(); ++i)
		{
			if (body[i] < '0' || body[i] > '9')
				continue;
			int id = 0;
			size_t after = i;
			if (ParseInt32(body, i, &id, &after) && id > 0)
				out.insert(id);
			i = (after > i) ? after - 1 : i;
		}
	}

	/* Convenience overloads for existing std::string call sites. */
	inline std::string StringAfterKey(const std::string& json, const char* key, size_t from = 0)
	{
		return StringAfterKey(AsView(json), View(key ? key : ""), from);
	}
	inline long long IntAfterKey(const std::string& json, const char* key, size_t from = 0)
	{
		return IntAfterKey(AsView(json), View(key ? key : ""), from);
	}
	inline bool BoolAfterKey(const std::string& json, const char* key, size_t from = 0)
	{
		return BoolAfterKey(AsView(json), View(key ? key : ""), from);
	}
	inline size_t ObjectEnd(const std::string& json, size_t openBrace)
	{
		return ObjectEnd(AsView(json), openBrace);
	}
	inline size_t ArrayEnd(const std::string& json, size_t openBracket)
	{
		return ArrayEnd(AsView(json), openBracket);
	}
	inline std::string ReadQuoted(const std::string& s, size_t openQuote, size_t* after = nullptr)
	{
		return ReadQuoted(AsView(s), openQuote, after);
	}
	inline bool CoordAfterKey(const std::string& json, const char* key, size_t from,
		float& outX, float& outY)
	{
		return CoordAfterKey(AsView(json), View(key ? key : ""), from, &outX, &outY);
	}
	inline void ParseIdArray(const std::string& body, std::unordered_set<int>& out)
	{
		ParseIdArray(AsView(body), out);
	}
}
