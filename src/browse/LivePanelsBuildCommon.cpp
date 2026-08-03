#include "LivePanelsBuildShared.h"

#include "LivePanels_Html.h"

#include "Gw2Http.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <windows.h>

namespace LivePanelsBuild
{
std::string WideToUtf8(const std::wstring& w)
{
	if (w.empty())
		return {};
	int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string out(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
	if (n > 0)
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
	return out;
}

std::string PathToFileUrl(const std::wstring& path)
{
	std::string utf8 = WideToUtf8(path);
	for (char& c : utf8)
	{
		if (c == '\\')
			c = '/';
	}
	if (utf8.size() >= 2 && utf8[1] == ':')
		return std::string("file:///") + utf8;
	return std::string("file://") + utf8;
}

std::wstring StemPath(const std::wstring& addonDir, const char* stem, const wchar_t* ext)
{
	std::wstring p = addonDir + L"\\";
	for (const char* s = stem; *s; ++s)
		p.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*s)));
	p += ext;
	return p;
}

bool FileFresh(const std::wstring& path, DWORD ttlSec)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	FILETIME ft{};
	const BOOL ok = GetFileTime(h, nullptr, nullptr, &ft);
	CloseHandle(h);
	if (!ok)
		return false;
	ULARGE_INTEGER u{};
	u.LowPart = ft.dwLowDateTime;
	u.HighPart = ft.dwHighDateTime;
	FILETIME nowFt{};
	GetSystemTimeAsFileTime(&nowFt);
	ULARGE_INTEGER n{};
	n.LowPart = nowFt.dwLowDateTime;
	n.HighPart = nowFt.dwHighDateTime;
	const ULONGLONG age100ns = (n.QuadPart > u.QuadPart) ? (n.QuadPart - u.QuadPart) : 0;
	const ULONGLONG ageSec = age100ns / 10000000ull;
	return ageSec <= ttlSec;
}

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
	if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 12 * 1024 * 1024)
	{
		CloseHandle(h);
		return {};
	}
	std::string out(static_cast<size_t>(sz.QuadPart), '\0');
	DWORD read = 0;
	const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
	CloseHandle(h);
	if (!ok)
		return {};
	out.resize(read);
	return out;
}

bool TryCacheHit(const std::wstring& addonDir, const char* stem, DWORD ttlSec,
	Gw2Http::Result& out)
{
	const std::wstring path = StemPath(addonDir, stem, L".json");
	if (!FileFresh(path, ttlSec))
		return false;
	std::string cached = ReadUtf8File(path);
	if (cached.empty())
		return false;
	out.ok = true;
	out.status = 200;
	out.body = std::move(cached);
	out.error.clear();
	return true;
}

void StoreCache(const std::wstring& addonDir, const char* stem, const Gw2Http::Result& r)
{
	if (r.ok && r.body.size() > 2)
		WriteUtf8File(StemPath(addonDir, stem, L".json"), r.body);
}

void PreferStaleCache(const std::wstring& addonDir, const char* stem, Gw2Http::Result& r)
{
	if (r.ok && r.body.size() > 2)
		return;
	std::string stale = ReadUtf8File(StemPath(addonDir, stem, L".json"));
	if (stale.empty())
		return;
	r.ok = true;
	r.status = 200;
	r.body = std::move(stale);
	r.error.clear();
}

DWORD WINAPI ParallelApiProc(void* param)
{
	auto* j = static_cast<ParallelApiJob*>(param);
	if (j && j->out && j->path && j->path[0])
		*j->out = Gw2Http::Api(j->path, j->bearer, j->timeoutMs);
	return 0;
}

/* Fire independent GETs together — wall clock ≈ slowest call, not sum.
   Cap is small (≤8) so we stay far under the ~600/min API budget. */
void RunParallelApis(ParallelApiJob* jobs, size_t n)
{
	if (!jobs || n == 0)
		return;
	if (n == 1)
	{
		ParallelApiProc(&jobs[0]);
		return;
	}
	std::vector<HANDLE> hs;
	hs.reserve(n);
	for (size_t i = 0; i < n; ++i)
	{
		HANDLE h = CreateThread(nullptr, 0, ParallelApiProc, &jobs[i], 0, nullptr);
		if (h)
			hs.push_back(h);
		else
			ParallelApiProc(&jobs[i]);
	}
	if (!hs.empty())
	{
		/* Wait in chunks of 64 (WaitForMultipleObjects limit). */
		size_t off = 0;
		while (off < hs.size())
		{
			const DWORD chunk = static_cast<DWORD>(
				(hs.size() - off > 64) ? 64 : (hs.size() - off));
			WaitForMultipleObjects(chunk, hs.data() + off, TRUE, 60000);
			off += chunk;
		}
		for (HANDLE h : hs)
			CloseHandle(h);
	}
}

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


std::string BuildPage(const char* title, const char* eyebrow, const char* heading,
	const char* tagline, const char* toc, const std::string& body, const std::string& extraHead)
{
	std::string out;
	out.reserve(body.size() + 4000);
	out += "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\"/>\n";
	out += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n<title>";
	out += title;
	out += "</title>\n<style>\n";
	out += LivePanelsHtml::SharedCss();
	out += "\n</style>\n";
	out += extraHead;
	out += "</head>\n<body>\n<div class=\"wrap\">\n<header class=\"hero\">\n";
	out += "<p class=\"eyebrow\">";
	out += eyebrow;
	out += "</p>\n<h1>";
	out += heading;
	out += "</h1>\n<p class=\"tagline\">";
	out += tagline;
	out += "</p>\n<p class=\"meta\">Updated ";
	out += HtmlEscape(NowLocalStamp());
	out += " · Reload tab for fresh data · Read-only API</p>\n</header>\n";
	if (toc && toc[0])
	{
		out += "<nav class=\"toc\" aria-label=\"Sections\">\n";
		out += toc;
		out += "\n</nav>\n";
	}
	out += body;
	out += "\n</div>\n</body>\n</html>\n";
	return out;
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

void AppendChecklistSection(std::string& body, const char* sectionId, const char* title,
	const char* blurb, const std::string& jsonArray)
{
	body += "<section class=\"block\" id=\"";
	body += sectionId;
	body += "\"><div class=\"head\"><h2>";
	body += title;
	body += "</h2><p>";
	body += blurb;
	body += "</p></div><div class=\"body\"><ul class=\"checks\">";
	int n = 0;
	size_t i = 0;
	while (n < 80 && i < jsonArray.size())
	{
		size_t q = jsonArray.find('"', i);
		if (q == std::string::npos)
			break;
		size_t after = q;
		std::string val = ReadJsonQuoted(jsonArray, q, &after);
		i = after;
		if (val.empty() || val == "id")
			continue;
		body += "<li><label class=\"check\"><input type=\"checkbox\"/>"
			"<span class=\"box\" aria-hidden=\"true\"></span><span class=\"txt\"><strong>";
		body += HtmlEscape(HumanizeApiId(val));
		body += "</strong></span></label></li>";
		++n;
	}
	body += "</ul>";
	if (n == 0)
		body += "<p class=\"note\">No data — hit Reload.</p>";
	else
		body += "<p class=\"meta\">Tick items off as you finish them (saved in this browser).</p>";
	body += "</div></section>\n";
}

void AppendVaultObjectives(std::string& body, const char* sectionId, const char* title,
	const std::string& json, bool accountScoped, const char* trackFilter,
	int maxItems)
{
	body += "<section class=\"block\" id=\"";
	body += sectionId;
	body += "\"><div class=\"head\"><h2>";
	body += title;
	body += "</h2><p>";
	body += accountScoped
		? "Live account progress from your API key — tick extras locally if you want."
		: "Catalog checklist (add an API key in Options for live personal progress).";
	body += "</p></div><div class=\"body\">";

	size_t pos = json.find('[');
	if (pos == std::string::npos)
	{
		body += "<p class=\"note\">No objectives in response.";
		if (accountScoped)
			body += " Check API key scopes (<strong>account</strong> + <strong>progression</strong>) in Nexus Options.";
		body += "</p></div></section>\n";
		return;
	}

	body += "<ul class=\"checks\">";
	int count = 0;
	size_t i = pos;
	while (count < maxItems)
	{
		size_t obj = json.find('{', i);
		if (obj == std::string::npos)
			break;
		size_t end = JsonObjectEnd(json, obj);
		if (end == std::string::npos)
			break;
		const std::string chunk = json.substr(obj, end - obj + 1);
		std::string titleStr = JsonStringAfterKey(chunk, "title");
		if (titleStr.empty())
			titleStr = JsonStringAfterKey(chunk, "name");
		std::string track = JsonStringAfterKey(chunk, "track");
		if (titleStr.empty())
		{
			i = end + 1;
			continue;
		}
		if (trackFilter && trackFilter[0] && track != trackFilter)
		{
			i = end + 1;
			continue;
		}
		long long cur = JsonIntAfterKey(chunk, "progress_current");
		long long need = JsonIntAfterKey(chunk, "progress_complete");
		long long acclaim = JsonIntAfterKey(chunk, "acclaim");
		bool claimed = JsonBoolAfterKey(chunk, "claimed");
		bool done = claimed || (need > 0 && cur >= need);

		body += "<li><label class=\"check\"><input type=\"checkbox\"";
		if (done)
			body += " checked";
		body += "/><span class=\"box\" aria-hidden=\"true\"></span><span class=\"txt\">";
		if (done)
			body += "<span class=\"badge\">Done</span>";
		if (!track.empty())
		{
			body += "<span class=\"badge\">";
			body += HtmlEscape(track);
			body += "</span>";
		}
		if (acclaim > 0 && acclaim <= 10)
			body += "<span class=\"badge\">Easy</span>";
		body += "<strong>";
		body += HtmlEscape(titleStr);
		body += "</strong>";
		if (acclaim > 0 || need > 0)
		{
			body += "<br/><span class=\"s\">";
			if (acclaim > 0)
			{
				body += std::to_string(acclaim);
				body += " acclaim";
			}
			if (need > 0)
			{
				if (acclaim > 0) body += " · ";
				body += "progress ";
				body += std::to_string(cur < 0 ? 0 : cur);
				body += " / ";
				body += std::to_string(need);
			}
			body += "</span>";
		}
		if (need > 0)
		{
			int pct = static_cast<int>((100.0 * (cur < 0 ? 0 : cur)) / need);
			if (pct > 100) pct = 100;
			if (pct < 0) pct = 0;
			body += "<div class=\"bar\"><i style=\"width:";
			body += std::to_string(pct);
			body += "%\"></i></div>";
		}
		body += "</span></label></li>";
		++count;
		i = end + 1;
	}
	body += "</ul>";
	if (count == 0)
		body += "<p class=\"note\">No parseable objectives.</p>";
	body += "</div></section>\n";
}

/* CEF page is informational only — add/remove/prices live in ImGui TpWatchPad
   (CEF file:// clicks and shared Live workers were unreliable under Wine). */
std::string BuildTpHtml(const char* /*tpWatchIds*/, bool /*fetchApi*/)
{
	std::string body;
	body += "<section class=\"block\"><div class=\"head\"><h2>TP Watchlist window</h2>"
		"<p>Use the ImGui panel — it is instant and reliable</p></div><div class=\"body\">";
	body += "<p class=\"note\">Your watchlist is managed in the <strong>TP Watchlist</strong> window "
		"(opens automatically from Browse → Live → TP Watchlist, or use the <strong>TP</strong> "
		"toolbar button / Nexus Options).</p>";
	body += "<ul class=\"rows\">";
	body += "<li><span class=\"t\">Add any item</span><span class=\"s\">"
		"In GW2: Shift+click an item → copy <code>[&…]</code> → paste in the TP window → Add. "
		"Numeric IDs also work.</span></li>";
	body += "<li><span class=\"t\">Prices</span><span class=\"s\">"
		"Buy / sell / spread refresh in that window (read-only official API, ~2s).</span></li>";
	body += "<li><span class=\"t\">Delivery box</span><span class=\"s\">"
		"Same window shows coins/items waiting to claim via <code>/v2/commerce/delivery</code> "
		"(API key with <strong>tradingpost</strong>). Reminder only — claim in-game.</span></li>";
	body += "</ul>";
	body += "<p style=\"margin-top:12px\"><a class=\"link\" href=\"https://www.gw2bltc.com/\">GW2BLTC</a>"
		" · <a class=\"link\" href=\"https://wiki.guildwars2.com/\">Wiki</a></p>";
	body += "</div></section>\n";
	return BuildPage(
		"Live — Trading Post Watchlist",
		"GW2 In-Game Helper · Live",
		"TP Watchlist",
		"Manage items in the TP Watchlist window beside the helper.",
		nullptr,
		body);
}

} // namespace LivePanelsBuild
