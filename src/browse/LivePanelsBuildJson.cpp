#include "LivePanelsBuildShared.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

namespace LivePanelsBuild
{
std::string HtmlEscape(const std::string& s)
{
	std::string o;
	o.reserve(s.size() + 8);
	for (unsigned char c : s)
	{
		switch (c)
		{
		case '&': o += "&amp;"; break;
		case '<': o += "&lt;"; break;
		case '>': o += "&gt;"; break;
		case '"': o += "&quot;"; break;
		case '\'': o += "&#39;"; break;
		default:
			if (c < 0x20 && c != '\n' && c != '\t')
				break;
			o.push_back(static_cast<char>(c));
			break;
		}
	}
	return o;
}

std::string NowLocalStamp()
{
	time_t t = time(nullptr);
	struct tm* tm = localtime(&t);
	char buf[64];
	if (!tm)
	{
		std::snprintf(buf, sizeof(buf), "unknown");
		return buf;
	}
	std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);
	return buf;
}

/* Minimal JSON helpers (GW2 API shapes only). */
std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from)
{
	std::string pat = "\"";
	pat += key;
	pat += "\"";
	size_t k = json.find(pat, from);
	if (k == std::string::npos)
		return {};
	k = json.find(':', k + pat.size());
	if (k == std::string::npos)
		return {};
	++k;
	while (k < json.size() && (json[k] == ' ' || json[k] == '\t' || json[k] == '\r' || json[k] == '\n'))
		++k;
	if (k >= json.size() || json[k] != '"')
		return {};
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
			else if (e == '"' || e == '\\' || e == '/') out.push_back(e);
			else if (e == 'u' && k + 3 < json.size())
			{
				/* skip \uXXXX — keep ASCII approximation */
				k += 4;
				out.push_back('?');
			}
			else
				out.push_back(e);
			continue;
		}
		if (c == '"')
			break;
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
	if (k == std::string::npos)
		return -1;
	k = json.find(':', k + pat.size());
	if (k == std::string::npos)
		return -1;
	++k;
	while (k < json.size() && (json[k] == ' ' || json[k] == '\t'))
		++k;
	bool neg = false;
	if (k < json.size() && json[k] == '-')
	{
		neg = true;
		++k;
	}
	long long v = 0;
	bool any = false;
	while (k < json.size() && json[k] >= '0' && json[k] <= '9')
	{
		any = true;
		v = v * 10 + (json[k] - '0');
		++k;
	}
	if (!any)
		return -1;
	return neg ? -v : v;
}

bool JsonBoolAfterKey(const std::string& json, const char* key, size_t from)
{
	std::string pat = "\"";
	pat += key;
	pat += "\"";
	size_t k = json.find(pat, from);
	if (k == std::string::npos)
		return false;
	k = json.find(':', k + pat.size());
	if (k == std::string::npos)
		return false;
	++k;
	while (k < json.size() && (json[k] == ' ' || json[k] == '\t'))
		++k;
	return k + 4 <= json.size() && json.compare(k, 4, "true") == 0;
}

std::string ExtractTagInner(const std::string& xml, const char* tag, size_t from, size_t* nextOut)
{
	std::string open = "<";
	open += tag;
	size_t a = xml.find(open, from);
	if (a == std::string::npos)
	{
		if (nextOut) *nextOut = std::string::npos;
		return {};
	}
	a = xml.find('>', a);
	if (a == std::string::npos)
	{
		if (nextOut) *nextOut = std::string::npos;
		return {};
	}
	++a;
	std::string close = "</";
	close += tag;
	close += ">";
	size_t b = xml.find(close, a);
	if (b == std::string::npos)
	{
		if (nextOut) *nextOut = std::string::npos;
		return {};
	}
	if (nextOut) *nextOut = b + close.size();
	std::string inner = xml.substr(a, b - a);
	/* strip CDATA */
	if (inner.size() >= 9 && inner.compare(0, 9, "<![CDATA[") == 0)
	{
		inner = inner.substr(9);
		if (inner.size() >= 3 && inner.compare(inner.size() - 3, 3, "]]>") == 0)
			inner.resize(inner.size() - 3);
	}
	return inner;
}

/* Find end of a JSON object starting at '{' — handles nested {} and strings. */
size_t JsonObjectEnd(const std::string& json, size_t openBrace)
{
	if (openBrace >= json.size() || json[openBrace] != '{')
		return std::string::npos;
	int depth = 0;
	bool inStr = false;
	bool esc = false;
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
			if (depth == 0)
				return i;
		}
	}
	return std::string::npos;
}

std::string ReadJsonQuoted(const std::string& s, size_t openQuote, size_t* after)
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
		char c = s[a++];
		if (c == '\\' && a < s.size())
		{
			val.push_back(s[a++]);
			continue;
		}
		if (c == '"')
			break;
		val.push_back(c);
	}
	if (after) *after = a;
	return val;
}

std::string FormatIsoDateUtc(const std::string& iso)
{
	/* "2026-09-01T16:00:00Z" → "September 1, 2026" */
	int y = 0, mo = 0, d = 0;
	if (std::sscanf(iso.c_str(), "%d-%d-%d", &y, &mo, &d) != 3 || y < 2000 || mo < 1 || mo > 12 || d < 1 || d > 31)
		return iso;
	static const char* kMonths[] = {
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"
	};
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%s %d, %d", kMonths[mo - 1], d, y);
	return buf;
}

bool ParseIsoUtc(const std::string& iso, time_t* out)
{
	int y = 0, mo = 0, d = 0, h = 0, mi = 0, s = 0;
	if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) < 3)
		return false;
	struct tm tm{};
	tm.tm_year = y - 1900;
	tm.tm_mon = mo - 1;
	tm.tm_mday = d;
	tm.tm_hour = h;
	tm.tm_min = mi;
	tm.tm_sec = s;
#ifdef _WIN32
	*out = _mkgmtime(&tm);
#else
	*out = timegm(&tm);
#endif
	return *out != (time_t)-1;
}

std::string SeasonDateBlurb(const std::string& startIso, const std::string& endIso)
{
	std::string out;
	time_t endT = 0;
	const bool hasEnd = !endIso.empty() && ParseIsoUtc(endIso, &endT);
	time_t startT = 0;
	const bool hasStart = !startIso.empty() && ParseIsoUtc(startIso, &startT);
	const time_t now = time(nullptr);

	if (hasEnd)
	{
		out += "Ends <strong>";
		out += HtmlEscape(FormatIsoDateUtc(endIso));
		out += "</strong>";
		const double days = difftime(endT, now) / 86400.0;
		if (days >= 0)
		{
			const int n = static_cast<int>(days + 0.999); /* ceil-ish remaining calendar days */
			out += " · <strong>";
			out += std::to_string(n);
			out += (n == 1 ? " day" : " days");
			out += " left</strong>";
		}
		else
			out += " · <strong>ended</strong>";
	}

	/* API start is often the first Vault season ever (Dec 2024), not this refresh.
	   Only show it when it looks like a real current-season start. */
	if (hasStart && hasEnd)
	{
		const double seasonLenDays = difftime(endT, startT) / 86400.0;
		const double ageDays = difftime(now, startT) / 86400.0;
		if (seasonLenDays > 0 && seasonLenDays <= 140 && ageDays >= -7 && ageDays <= 140)
		{
			if (!out.empty()) out += "<br/>";
			out += "Started ";
			out += HtmlEscape(FormatIsoDateUtc(startIso));
		}
	}
	else if (hasStart && !hasEnd)
	{
		out += "Started ";
		out += HtmlEscape(FormatIsoDateUtc(startIso));
	}

	if (out.empty())
		out = "Season dates unavailable.";
	return out;
}

std::string HumanizeApiId(const std::string& id)
{
	std::string out;
	out.reserve(id.size() + 4);
	bool cap = true;
	for (unsigned char c : id)
	{
		if (c == '_' || c == '-')
		{
			out.push_back(' ');
			cap = true;
			continue;
		}
		if (cap && c >= 'a' && c <= 'z')
			out.push_back(static_cast<char>(c - 'a' + 'A'));
		else
			out.push_back(static_cast<char>(c));
		cap = false;
	}
	return out;
}

} // namespace LivePanelsBuild
