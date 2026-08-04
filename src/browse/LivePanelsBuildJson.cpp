#include "LivePanelsBuildShared.h"

#include "JsonView.h"

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

/* Minimal JSON helpers (GW2 API shapes only) — bounds-checked via JsonView. */
std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from)
{
	return JsonView::StringAfterKey(json, key, from);
}

long long JsonIntAfterKey(const std::string& json, const char* key, size_t from)
{
	return JsonView::IntAfterKey(json, key, from);
}

bool JsonBoolAfterKey(const std::string& json, const char* key, size_t from)
{
	return JsonView::BoolAfterKey(json, key, from);
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

size_t JsonObjectEnd(const std::string& json, size_t openBrace)
{
	return JsonView::ObjectEnd(json, openBrace);
}

std::string ReadJsonQuoted(const std::string& s, size_t openQuote, size_t* after)
{
	return JsonView::ReadQuoted(s, openQuote, after);
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
