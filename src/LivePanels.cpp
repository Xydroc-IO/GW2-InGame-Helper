#include "LivePanels.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "Gw2Http.h"
#include "Settings.h"
#include "WikiBrowser.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace
{
	constexpr const char* kPanelVer = "18";
	constexpr DWORD kHtmlTtlSec = 10u * 60u;       /* avoid rebuild storms */
	constexpr DWORD kTpHtmlTtlSec = 60u;
	constexpr DWORD kColorsTtlSec = 7u * 24u * 60u * 60u;
	constexpr DWORD kArmoryTtlSec = 24u * 60u * 60u;
	constexpr DWORD kPublicTtlSec = 60u * 60u;      /* craft / bosses / vault objs */
	constexpr DWORD kSeasonTtlSec = 6u * 60u * 60u;
	constexpr DWORD kAccountTtlSec = 3u * 60u;      /* personal vault / armory / chars */
	/* Per-request budget on workers only — UI always gets a shell instantly. */
	constexpr int kLiveHttpTimeoutMs = 4000;
	constexpr int kLiveBulkTimeoutMs = 8000;
	constexpr int kMaxLiveWorkers = 3;

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

	struct ParallelApiJob
	{
		const char* path = nullptr;
		const char* bearer = nullptr;
		int timeoutMs = kLiveHttpTimeoutMs;
		Gw2Http::Result* out = nullptr;
	};

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
	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from = 0)
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

	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from = 0)
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

	bool JsonBoolAfterKey(const std::string& json, const char* key, size_t from = 0)
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

	static const char* kCss = R"CSS(
  :root {
    --bg: #06070a; --panel: rgba(16, 18, 24, 0.92); --panel-2: #12141a;
    --border: #5a4a28; --border-soft: rgba(235, 192, 71, 0.22);
    --gold: #f0c65a; --gold-bright: #ffe08a; --gold-dim: #c9a227;
    --text: #f0f2f5; --muted: #a8aeb8; --accent: #1a1510;
    --ok: #6aaa6a; --warn: #c9a227;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0; min-height: 100vh;
    font-family: "Segoe UI", Tahoma, sans-serif;
    background: radial-gradient(ellipse 80% 55% at 50% 0%, rgba(235, 192, 71, 0.12) 0%, transparent 55%),
      linear-gradient(180deg, #12141a 0%, var(--bg) 45%), var(--bg);
    color: var(--text); line-height: 1.55;
  }
  .wrap { max-width: 900px; margin: 0 auto; padding: 28px 22px 64px; }
  .hero { margin-bottom: 18px; padding-bottom: 16px; border-bottom: 1px solid var(--border-soft); }
  .eyebrow { margin: 0 0 8px; font-size: 0.78rem; letter-spacing: 0.14em; text-transform: uppercase; color: var(--gold-dim); }
  h1 { margin: 0 0 8px; font-size: 1.85rem; font-weight: 700; color: var(--gold); }
  .tagline { margin: 0; color: var(--muted); font-size: 0.98rem; }
  .meta { margin: 10px 0 0; font-size: 0.82rem; color: var(--muted); }
  nav.toc { display: flex; flex-wrap: wrap; gap: 8px; margin: 0 0 20px; }
  nav.toc a {
    color: var(--gold); text-decoration: none; font-size: 0.86rem;
    padding: 6px 10px; border: 1px solid var(--border); background: var(--accent);
  }
  section.block { background: var(--panel); border: 1px solid var(--border); margin-bottom: 16px; }
  section.block > .head {
    padding: 12px 16px; border-bottom: 1px solid var(--border-soft); border-left: 3px solid var(--gold);
    background: linear-gradient(90deg, #1a1710 0%, var(--panel-2) 70%);
  }
  section.block > .head h2 { margin: 0; font-size: 1.12rem; color: var(--gold-bright); }
  section.block > .head p { margin: 4px 0 0; font-size: 0.88rem; color: var(--muted); }
  .body { padding: 14px 16px 16px; }
  .note {
    margin: 0 0 12px; padding: 10px 12px; background: var(--accent);
    border-left: 3px solid var(--gold-dim); color: var(--muted); font-size: 0.9rem;
  }
  .note strong { color: var(--text); }
  ul.rows { list-style: none; margin: 0; padding: 0; }
  ul.rows li {
    padding: 10px 12px; margin: 0 0 8px; background: var(--panel-2);
    border: 1px solid var(--border-soft); border-left: 3px solid var(--gold-dim);
  }
  ul.rows li.done { border-left-color: var(--ok); opacity: 0.85; }
  ul.rows .t { font-weight: 650; color: var(--text); }
  ul.rows .s { display: block; margin-top: 4px; font-size: 0.86rem; color: var(--muted); }
  .bar {
    margin-top: 8px; height: 8px; background: #0c0d10; border: 1px solid var(--border-soft);
  }
  .bar > i { display: block; height: 100%; background: var(--gold-dim); }
  .badge {
    display: inline-block; padding: 2px 7px; font-size: 0.7rem; font-weight: 700;
    letter-spacing: 0.05em; text-transform: uppercase; border: 1px solid var(--border);
    background: var(--accent); color: var(--gold-dim); margin-right: 6px;
  }
  a.link { color: var(--gold); }
  input#dyeFilter {
    width: 100%; padding: 10px 12px; margin: 0 0 12px; border: 1px solid var(--border);
    background: var(--accent); color: var(--text); font-size: 0.95rem;
  }
  .checks { list-style: none; margin: 0; padding: 0; }
  .checks li { margin: 0 0 8px; }
  .check { display: flex; gap: 10px; align-items: flex-start; cursor: pointer; }
  .check input { margin-top: 4px; flex-shrink: 0; }
  .check .box {
    display: none;
  }
  .check .txt { color: var(--muted); flex: 1; }
  .check .txt strong { color: var(--text); }
  .check input:checked + .box + .txt strong,
  .check:has(input:checked) .txt { opacity: 0.75; }
  .swatch {
    display: inline-block; width: 14px; height: 14px; border: 1px solid #000;
    vertical-align: -2px; margin-right: 6px;
  }
  .keybox {
    margin: 0 0 16px; padding: 14px 16px; border: 1px solid var(--border);
    background: linear-gradient(90deg, #1a1710 0%, var(--panel-2) 70%);
  }
  .keybox.ok { border-left: 3px solid var(--ok); }
  .keybox.warn { border-left: 3px solid var(--warn); }
  .keybox h3 { margin: 0 0 6px; color: var(--gold-bright); font-size: 1.05rem; }
  .keybox p { margin: 0; color: var(--muted); font-size: 0.9rem; }
  .keybox strong { color: var(--text); }
  .muted { color: var(--muted); font-size: 0.86rem; }
  code { color: var(--gold-dim); font-size: 0.88rem; }
)CSS";

	std::string BuildPage(const char* title, const char* eyebrow, const char* heading,
		const char* tagline, const char* toc, const std::string& body, const std::string& extraHead = {})
	{
		std::string out;
		out.reserve(body.size() + 4000);
		out += "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\"/>\n";
		out += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>\n<title>";
		out += title;
		out += "</title>\n<style>\n";
		out += kCss;
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
		const std::string& json, bool accountScoped, const char* trackFilter = nullptr,
		int maxItems = 80)
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

	std::string BuildDailiesHtml(const std::wstring& addonDir, const char* apiKey)
	{
		const bool hasKey = apiKey && apiKey[0];
		std::string body;

		if (hasKey)
		{
			body += "<div class=\"keybox ok\" id=\"apikey\"><h3>API key connected</h3>"
				"<p>Showing <strong>live</strong> Wizard’s Vault progress for your account "
				"(same read-only endpoints Discord bots use). "
				"Change or clear the key in <strong>Nexus Options → GW2 In-Game Helper</strong>, then Reload.</p></div>";
		}
		else
		{
			body += "<div class=\"keybox warn\" id=\"apikey\"><h3>Add your GW2 API key for live Vault</h3>"
				"<p>1. Open <strong>Nexus → Options → GW2 In-Game Helper</strong><br/>"
				"2. Paste a key from <strong>account.arena.net/applications</strong> with "
				"<strong>account</strong> + <strong>progression</strong><br/>"
				"3. Come back here and hit <strong>Reload</strong><br/>"
				"The key stays in this addon’s local settings.ini only — never sent to third parties.</p></div>";
		}

		/* Cache-first, then one parallel round for misses (typically 0–6 GETs). */
		Gw2Http::Result season, craft, bosses, vd, vw, vs, pub;
		const bool hitSeason = TryCacheHit(addonDir, "live-season", kSeasonTtlSec, season);
		const bool hitCraft = TryCacheHit(addonDir, "live-craft", kPublicTtlSec, craft);
		const bool hitBosses = TryCacheHit(addonDir, "live-bosses", kPublicTtlSec, bosses);
		bool hitVd = false, hitVw = false, hitVs = false, hitPub = false;
		if (hasKey)
		{
			hitVd = TryCacheHit(addonDir, "live-vault-daily", kAccountTtlSec, vd);
			hitVw = TryCacheHit(addonDir, "live-vault-weekly", kAccountTtlSec, vw);
			hitVs = TryCacheHit(addonDir, "live-vault-special", kAccountTtlSec, vs);
		}
		else
			hitPub = TryCacheHit(addonDir, "live-vault-obj", kPublicTtlSec, pub);

		ParallelApiJob jobs[8];
		size_t nJobs = 0;
		auto enqueue = [&](const char* path, const char* bearer, int timeoutMs, Gw2Http::Result* out) {
			if (nJobs >= 8 || !path || !out)
				return;
			jobs[nJobs].path = path;
			jobs[nJobs].bearer = bearer;
			jobs[nJobs].timeoutMs = timeoutMs;
			jobs[nJobs].out = out;
			++nJobs;
		};
		if (!hitSeason)
			enqueue("/v2/wizardsvault", nullptr, kLiveHttpTimeoutMs, &season);
		if (!hitCraft)
			enqueue("/v2/dailycrafting", nullptr, kLiveHttpTimeoutMs, &craft);
		if (!hitBosses)
			enqueue("/v2/worldbosses", nullptr, kLiveHttpTimeoutMs, &bosses);
		if (hasKey)
		{
			if (!hitVd)
				enqueue("/v2/account/wizardsvault/daily", apiKey, kLiveHttpTimeoutMs, &vd);
			if (!hitVw)
				enqueue("/v2/account/wizardsvault/weekly", apiKey, kLiveHttpTimeoutMs, &vw);
			if (!hitVs)
				enqueue("/v2/account/wizardsvault/special", apiKey, kLiveHttpTimeoutMs, &vs);
		}
		else if (!hitPub)
			enqueue("/v2/wizardsvault/objectives?ids=all", nullptr, kLiveBulkTimeoutMs, &pub);

		RunParallelApis(jobs, nJobs);

		if (!hitSeason) { StoreCache(addonDir, "live-season", season); PreferStaleCache(addonDir, "live-season", season); }
		if (!hitCraft) { StoreCache(addonDir, "live-craft", craft); PreferStaleCache(addonDir, "live-craft", craft); }
		if (!hitBosses) { StoreCache(addonDir, "live-bosses", bosses); PreferStaleCache(addonDir, "live-bosses", bosses); }
		if (hasKey)
		{
			if (!hitVd) { StoreCache(addonDir, "live-vault-daily", vd); PreferStaleCache(addonDir, "live-vault-daily", vd); }
			if (!hitVw) { StoreCache(addonDir, "live-vault-weekly", vw); PreferStaleCache(addonDir, "live-vault-weekly", vw); }
			if (!hitVs) { StoreCache(addonDir, "live-vault-special", vs); PreferStaleCache(addonDir, "live-vault-special", vs); }
		}
		else if (!hitPub)
		{
			StoreCache(addonDir, "live-vault-obj", pub);
			PreferStaleCache(addonDir, "live-vault-obj", pub);
		}

		body += "<section class=\"block\" id=\"season\"><div class=\"head\"><h2>Current season</h2>"
			"<p>/v2/wizardsvault</p></div><div class=\"body\">";
		if (season.ok)
		{
			std::string title = JsonStringAfterKey(season.body, "title");
			std::string start = JsonStringAfterKey(season.body, "start");
			std::string end = JsonStringAfterKey(season.body, "end");
			body += "<p class=\"t\" style=\"font-size:1.2rem;font-weight:700;color:var(--gold)\">";
			body += HtmlEscape(title.empty() ? "Wizard’s Vault" : title);
			body += "</p>";
			body += "<p class=\"s\">";
			body += SeasonDateBlurb(start, end);
			body += "</p>";
		}
		else
			body += "<p class=\"note\">Season fetch failed: " + HtmlEscape(season.error) + "</p>";
		body += "</div></section>\n";

		if (hasKey)
		{
			if (vd.ok)
				AppendVaultObjectives(body, "vault-daily", "Your daily Vault (live)", vd.body, true);
			else
				body += "<section class=\"block\" id=\"vault-daily\"><div class=\"head\"><h2>Your daily Vault (live)</h2></div>"
					"<div class=\"body\"><p class=\"note\">Failed: " +
					HtmlEscape(vd.error) + " (HTTP " + std::to_string(vd.status) +
					"). Confirm scopes <strong>account</strong> + <strong>progression</strong>, then Reload.</p></div></section>\n";
			if (vw.ok)
				AppendVaultObjectives(body, "vault-weekly", "Your weekly Vault (live)", vw.body, true);
			else
				body += "<section class=\"block\" id=\"vault-weekly\"><div class=\"head\"><h2>Your weekly Vault (live)</h2></div>"
					"<div class=\"body\"><p class=\"note\">Failed: " + HtmlEscape(vw.error) + "</p></div></section>\n";
			if (vs.ok)
				AppendVaultObjectives(body, "vault-special", "Your special Vault (live)", vs.body, true);
		}
		else
		{
			if (pub.ok && pub.body.find("\"title\"") != std::string::npos)
			{
				std::string easyOnly = "[";
				size_t i = 0;
				int easyN = 0;
				bool first = true;
				while (easyN < 60)
				{
					size_t obj = pub.body.find('{', i);
					if (obj == std::string::npos) break;
					size_t end = JsonObjectEnd(pub.body, obj);
					if (end == std::string::npos) break;
					std::string chunk = pub.body.substr(obj, end - obj + 1);
					long long ac = JsonIntAfterKey(chunk, "acclaim");
					if (ac > 0 && ac <= 10)
					{
						if (!first) easyOnly += ',';
						easyOnly += chunk;
						first = false;
						++easyN;
					}
					i = end + 1;
				}
				easyOnly += ']';
				AppendVaultObjectives(body, "vault-easy", "Easy Vault objectives (preview)", easyOnly, false);
			}
			else
			{
				body += "<section class=\"block\" id=\"vault-easy\"><div class=\"head\"><h2>Wizard’s Vault</h2></div>"
					"<div class=\"body\"><p class=\"note\">Add an API key above for live personal objectives, or Reload.</p></div></section>\n";
			}
		}

		if (craft.ok)
			AppendChecklistSection(body, "craft", "Daily crafting",
				"Today’s time-gated crafts — tick them off as you go.", craft.body);
		else
			body += "<section class=\"block\" id=\"craft\"><div class=\"head\"><h2>Daily crafting</h2></div>"
				"<div class=\"body\"><p class=\"note\">Could not load (" + HtmlEscape(craft.error) + ").</p></div></section>\n";

		if (bosses.ok)
			AppendChecklistSection(body, "bosses", "World bosses",
				"World-boss checklist for today — tick as you finish. Use GW2Timer for countdowns.", bosses.body);
		else
			body += "<section class=\"block\" id=\"bosses\"><div class=\"head\"><h2>World bosses</h2></div>"
				"<div class=\"body\"><p class=\"note\">Could not load (" + HtmlEscape(bosses.error) + ").</p></div></section>\n";

		body += "<section class=\"block\" id=\"links\"><div class=\"head\"><h2>Related</h2></div><div class=\"body\">"
			"<ul class=\"rows\">"
			"<li><a class=\"link\" href=\"about:daily-weekly\">Offline Daily / Weekly checklist</a></li>"
			"<li><a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Wizard%27s_Vault/Easy_objectives\">Wiki — Easy Vault objectives</a></li>"
			"<li><a class=\"link\" href=\"https://gw2timer.com/\">GW2Timer — live schedules</a></li>"
			"<li><a class=\"link\" href=\"https://account.arena.net/applications\">Create / manage API keys</a></li>"
			"</ul></div></section>\n";

		return BuildPage(
			"Live — Dailies &amp; Vault",
			"GW2 In-Game Helper · Live",
			"Dailies &amp; Wizard’s Vault",
			hasKey
				? "Live Vault progress from your API key, plus crafting and world-boss checklists."
				: "Add your API key in Nexus Options for live Vault — crafting and bosses work now.",
			"<a href=\"#apikey\">API key</a>\n<a href=\"#vault-daily\">Vault</a>\n"
			"<a href=\"#craft\">Crafting</a>\n<a href=\"#bosses\">Bosses</a>\n<a href=\"#links\">Links</a>",
			body);
	}

	std::string BuildNewsHtml()
	{
		std::string body;
		auto build = Gw2Http::Api("/v2/build", nullptr, kLiveHttpTimeoutMs);
		long long buildId = build.ok ? JsonIntAfterKey(build.body, "id") : -1;
		body += "<section class=\"block\" id=\"build\"><div class=\"head\"><h2>Client build</h2>"
			"<p>Official API /v2/build</p></div><div class=\"body\">";
		if (buildId > 0)
		{
			body += "<p class=\"t\" style=\"font-size:1.4rem;font-weight:700;color:var(--gold)\">Build ";
			body += std::to_string(buildId);
			body += "</p>";
		}
		else
			body += "<p class=\"note\">Could not read build id (" + HtmlEscape(build.error) + ").</p>";
		body += "</div></section>\n";

		body += "<section class=\"block\" id=\"news\"><div class=\"head\"><h2>Official news</h2>"
			"<p>RSS from guildwars2.com</p></div><div class=\"body\">";
		auto feed = Gw2Http::Get("https://www.guildwars2.com/en/feed/", nullptr, kLiveHttpTimeoutMs);
		if (!feed.ok)
			feed = Gw2Http::Get("https://www.guildwars2.com/en/rss.xml", nullptr, kLiveHttpTimeoutMs);
		int newsCount = 0;
		if (feed.ok)
		{
			body += "<ul class=\"rows\">";
			size_t pos = 0;
			while (newsCount < 12)
			{
				size_t itemStart = feed.body.find("<item", pos);
				if (itemStart == std::string::npos)
					break;
				size_t itemEnd = feed.body.find("</item>", itemStart);
				if (itemEnd == std::string::npos)
					break;
				std::string item = feed.body.substr(itemStart, itemEnd - itemStart);
				size_t npos = 0;
				std::string title = ExtractTagInner(item, "title", 0, &npos);
				std::string link = ExtractTagInner(item, "link", 0, &npos);
				std::string date = ExtractTagInner(item, "pubDate", 0, &npos);
				if (title.empty())
				{
					pos = itemEnd + 7;
					continue;
				}
				body += "<li><span class=\"t\">";
				if (!link.empty())
				{
					body += "<a class=\"link\" href=\"";
					body += HtmlEscape(link);
					body += "\">";
					body += HtmlEscape(title);
					body += "</a>";
				}
				else
					body += HtmlEscape(title);
				body += "</span>";
				if (!date.empty())
				{
					body += "<span class=\"s\">";
					body += HtmlEscape(date);
					body += "</span>";
				}
				body += "</li>";
				++newsCount;
				pos = itemEnd + 7;
			}
			body += "</ul>";
		}
		if (newsCount == 0)
		{
			body += "<p class=\"note\">RSS unavailable";
			if (!feed.ok)
			{
				body += " (";
				body += HtmlEscape(feed.error);
				body += ")";
			}
			body += ". Open the official news page below.</p>";
		}
		body += "<p style=\"margin-top:12px\"><a class=\"link\" href=\"https://www.guildwars2.com/en/news/\">Official News</a>"
			" · <a class=\"link\" href=\"https://en-forum.guildwars2.com/\">Forums</a></p>";
		body += "</div></section>\n";

		body += "<section class=\"block\" id=\"updates\"><div class=\"head\"><h2>Wiki — Game updates</h2>"
			"<p>MediaWiki sections</p></div><div class=\"body\">";
		auto wiki = Gw2Http::Get(
			"https://wiki.guildwars2.com/api.php?action=parse&page=Game_updates&prop=sections&format=json&formatversion=2",
			nullptr, kLiveHttpTimeoutMs);
		int secCount = 0;
		if (wiki.ok)
		{
			body += "<ul class=\"rows\">";
			size_t p = 0;
			while (secCount < 15)
			{
				size_t line = wiki.body.find("\"line\":", p);
				if (line == std::string::npos)
					break;
				std::string lineStr = JsonStringAfterKey(wiki.body, "line", line);
				long long level = JsonIntAfterKey(wiki.body, "level", line > 80 ? line - 80 : 0);
				std::string anchor = JsonStringAfterKey(wiki.body, "anchor", line);
				p = line + 7;
				if (lineStr.empty())
					continue;
				if (level > 0 && level > 3)
					continue;
				body += "<li><span class=\"t\"><a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Game_updates";
				if (!anchor.empty())
				{
					body += "#";
					body += HtmlEscape(anchor);
				}
				body += "\">";
				body += HtmlEscape(lineStr);
				body += "</a></span></li>";
				++secCount;
			}
			body += "</ul>";
		}
		if (secCount == 0)
		{
			body += "<p class=\"note\">Could not load wiki sections. "
				"<a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Game_updates\">Open Game updates</a>.</p>";
		}
		else
		{
			body += "<p style=\"margin-top:12px\"><a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Game_updates\">Full Game updates page</a></p>";
		}
		body += "</div></section>\n";

		return BuildPage(
			"Live — News &amp; Patch Digest",
			"GW2 In-Game Helper · Live",
			"News &amp; Patch Digest",
			"Official build id, news headlines, and wiki Game updates — one place.",
			"<a href=\"#build\">Build</a>\n<a href=\"#news\">News</a>\n<a href=\"#updates\">Updates</a>",
			body);
	}

	struct InfusionRow { const char* name; const char* url; };

	const InfusionRow kInfusions[] = {
		{ "Arcane Flow Infusion", "https://wiki.guildwars2.com/wiki/Arcane_Flow_Infusion" },
		{ "Forest Wisp Infusion", "https://wiki.guildwars2.com/wiki/Forest_Wisp_Infusion" },
		{ "Mystic Infusion", "https://wiki.guildwars2.com/wiki/Mystic_Infusion" },
		{ "Queen Bee Infusion", "https://wiki.guildwars2.com/wiki/Queen_Bee_Infusion" },
		{ "Liquid Aurillium Infusion", "https://wiki.guildwars2.com/wiki/Liquid_Aurillium_Infusion" },
		{ "Chak Infusion", "https://wiki.guildwars2.com/wiki/Chak_Infusion" },
		{ "Festive Confetti Infusion", "https://wiki.guildwars2.com/wiki/Festive_Confetti_Infusion" },
		{ "Crystal Infusion", "https://wiki.guildwars2.com/wiki/Crystal_Infusion" },
		{ "Heart of the Khan-Ur", "https://wiki.guildwars2.com/wiki/Heart_of_the_Khan-Ur" },
		{ "Frost Legion Infusion", "https://wiki.guildwars2.com/wiki/Frost_Legion_Infusion" },
		{ "Jormag Left Eye Infusion", "https://wiki.guildwars2.com/wiki/Jormag_Left_Eye_Infusion" },
		{ "Jormag Right Eye Infusion", "https://wiki.guildwars2.com/wiki/Jormag_Right_Eye_Infusion" },
		{ "Echo of the Dragonvoid", "https://wiki.guildwars2.com/wiki/Echo_of_the_Dragonvoid" },
		{ "Chromatic Bubbles", "https://wiki.guildwars2.com/wiki/Chromatic_Bubbles" },
		{ "Seer Transcendence", "https://wiki.guildwars2.com/wiki/Seer_Transcendence" },
		{ "Ethereal Sea-Life Infusion", "https://wiki.guildwars2.com/wiki/Ethereal_Sea-Life_Infusion" },
		{ "Celestial Infusion (Blue)", "https://wiki.guildwars2.com/wiki/Celestial_Infusion_(Blue)" },
		{ "Celestial Infusion (Red)", "https://wiki.guildwars2.com/wiki/Celestial_Infusion_(Red)" },
		{ "Abyssal Infusion", "https://wiki.guildwars2.com/wiki/Abyssal_Infusion" },
		{ "Ghostly Infusion", "https://wiki.guildwars2.com/wiki/Ghostly_Infusion" },
		{ "Peerless Infusion", "https://wiki.guildwars2.com/wiki/Peerless_Infusion" },
		{ "Imperial Everbloom", "https://wiki.guildwars2.com/wiki/Imperial_Everbloom" },
		{ "Clockwork Infusion", "https://wiki.guildwars2.com/wiki/Clockwork_Infusion" },
		{ "Demonic Infusion", "https://wiki.guildwars2.com/wiki/Demonic_Infusion" },
		{ "Deldrimor Stoneskin Infusion", "https://wiki.guildwars2.com/wiki/Deldrimor_Stoneskin_Infusion" },
		{ "Ember Infusion", "https://wiki.guildwars2.com/wiki/Ember_Infusion" },
		{ "Winter's Heart Infusion", "https://wiki.guildwars2.com/wiki/Winter%27s_Heart_Infusion" },
		{ "Snow Diamond Infusion", "https://wiki.guildwars2.com/wiki/Snow_Diamond_Infusion" },
		{ "Mistwalker Infusion", "https://wiki.guildwars2.com/wiki/Mistwalker_Infusion" },
		{ "Heat Core Infusion", "https://wiki.guildwars2.com/wiki/Heat_Core_Infusion" },
	};

	std::string EnsureColorsJson(const std::wstring& addonDir)
	{
		const std::wstring path = StemPath(addonDir, "live-colors", L".json");
		if (FileFresh(path, kColorsTtlSec))
		{
			std::string cached = ReadUtf8File(path);
			if (!cached.empty())
				return cached;
		}
		/* Cached to disk for a week — keep budget modest so a cold miss cannot
		   pin a Live worker for tens of seconds. */
		auto r = Gw2Http::Api("/v2/colors?ids=all", nullptr, 6000);
		if (r.ok && r.body.size() > 10)
		{
			WriteUtf8File(path, r.body);
			return r.body;
		}
		std::string stale = ReadUtf8File(path);
		return stale;
	}

	std::string BuildFashionHtml(const std::wstring& addonDir)
	{
		std::string colors = EnsureColorsJson(addonDir);
		std::string body;
		body += "<p class=\"note\">Checklist is local (browser storage) — wishlist / planning only. "
			"No wardrobe or game-memory reads. Tick dyes and infusions you want; Reload refreshes the dye catalog.</p>";

		body += "<section class=\"block\" id=\"dyes\"><div class=\"head\"><h2>Dye wishlist</h2>"
			"<p>From official /v2/colors — filter and tick</p></div><div class=\"body\">";
		body += "<input id=\"dyeFilter\" type=\"search\" placeholder=\"Filter dyes…\" autocomplete=\"off\"/>";
		body += "<ul class=\"checks\" id=\"dyeList\">";

		int dyeCount = 0;
		if (!colors.empty())
		{
			size_t p = 0;
			while (dyeCount < 2000)
			{
				size_t idKey = colors.find("\"id\"", p);
				if (idKey == std::string::npos)
					break;
				long long id = JsonIntAfterKey(colors, "id", idKey);
				size_t nameKey = colors.find("\"name\"", idKey);
				size_t nextId = colors.find("\"id\"", idKey + 4);
				if (id < 0 || nameKey == std::string::npos ||
					(nextId != std::string::npos && nameKey > nextId))
				{
					p = idKey + 4;
					continue;
				}
				std::string name = JsonStringAfterKey(colors, "name", nameKey);
				if (name.empty())
				{
					p = idKey + 4;
					continue;
				}

				char rgb[32] = "888888";
				auto tryParseRgbArray = [&](size_t fromKey, size_t limit) -> bool {
					if (fromKey == std::string::npos || fromKey >= limit)
						return false;
					size_t br = colors.find('[', fromKey);
					if (br == std::string::npos || br >= limit)
						return false;
					int rv = 0, gv = 0, bv = 0;
					/* Pretty-printed arrays: [\\n 54,\\n 130,\\n 160\\n] — %d skips whitespace. */
					if (std::sscanf(colors.c_str() + br, "[%d,%d,%d]", &rv, &gv, &bv) != 3)
						return false;
					if (rv < 0) rv = 0;
					if (rv > 255) rv = 255;
					if (gv < 0) gv = 0;
					if (gv > 255) gv = 255;
					if (bv < 0) bv = 0;
					if (bv > 255) bv = 255;
					std::snprintf(rgb, sizeof(rgb), "%02x%02x%02x", rv, gv, bv);
					return true;
				};

				/* base_rgb is often a shared red placeholder; cloth/leather/metal hold the real swatch. */
				const size_t colorEnd = (nextId != std::string::npos) ? nextId : colors.size();
				size_t cloth = colors.find("\"cloth\"", idKey);
				size_t leather = colors.find("\"leather\"", idKey);
				size_t metal = colors.find("\"metal\"", idKey);
				size_t clothRgb = (cloth != std::string::npos && cloth < colorEnd)
					? colors.find("\"rgb\"", cloth) : std::string::npos;
				size_t leatherRgb = (leather != std::string::npos && leather < colorEnd)
					? colors.find("\"rgb\"", leather) : std::string::npos;
				size_t metalRgb = (metal != std::string::npos && metal < colorEnd)
					? colors.find("\"rgb\"", metal) : std::string::npos;
				size_t baseRgb = colors.find("\"base_rgb\"", idKey);
				if (baseRgb != std::string::npos && baseRgb >= colorEnd)
					baseRgb = std::string::npos;

				if (!tryParseRgbArray(clothRgb, colorEnd) &&
					!tryParseRgbArray(leatherRgb, colorEnd) &&
					!tryParseRgbArray(metalRgb, colorEnd))
					tryParseRgbArray(baseRgb, colorEnd);

				std::string lower = name;
				for (char& c : lower)
					if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');

				body += "<li class=\"dye\" data-name=\"";
				body += HtmlEscape(lower);
				body += "\"><label class=\"check\"><input type=\"checkbox\"/>";
				body += "<span class=\"box\" aria-hidden=\"true\"></span><span class=\"txt\">";
				body += "<span class=\"swatch\" style=\"background:#";
				body += rgb;
				body += "\"></span><strong>";
				body += HtmlEscape(name);
				body += "</strong> <span class=\"muted\">#";
				body += std::to_string(id);
				body += "</span></span></label></li>";
				++dyeCount;
				p = (nextId != std::string::npos) ? nextId : colors.size();
			}
		}
		body += "</ul>";
		if (dyeCount == 0)
			body += "<p class=\"note\">Dye catalog unavailable (network). Infusions below still work.</p>";
		else
		{
			body += "<p class=\"meta\">";
			body += std::to_string(dyeCount);
			body += " dyes loaded.</p>";
		}
		body += "</div></section>\n";

		body += "<section class=\"block\" id=\"infusions\"><div class=\"head\"><h2>Infusion wishlist</h2>"
			"<p>Curated wiki links — tick what you want</p></div><div class=\"body\"><ul class=\"checks\">";
		for (const InfusionRow& row : kInfusions)
		{
			body += "<li><label class=\"check\"><input type=\"checkbox\"/>"
				"<span class=\"box\" aria-hidden=\"true\"></span><span class=\"txt\"><strong>";
			body += HtmlEscape(row.name);
			body += "</strong> — <a class=\"link\" href=\"";
			body += row.url;
			body += "\">wiki</a></span></label></li>";
		}
		body += "</ul>"
			"<p style=\"margin-top:12px\"><a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Cosmetic_infusion\">"
			"All cosmetic infusions (wiki)</a></p></div></section>\n";

		const char* filterJs =
			"<script>\n"
			"(function(){var f=document.getElementById('dyeFilter');var list=document.getElementById('dyeList');"
			"if(!f||!list)return;f.addEventListener('input',function(){var q=(f.value||'').toLowerCase();"
			"var items=list.querySelectorAll('li.dye');for(var i=0;i<items.length;i++){"
			"var n=items[i].getAttribute('data-name')||'';items[i].style.display=!q||n.indexOf(q)>=0?'':'none';}});})();\n"
			"</script>\n";

		return BuildPage(
			"Live — Fashion Wishlist",
			"GW2 In-Game Helper · Live",
			"Fashion Wishlist",
			"Dyes and cosmetic infusions — plan looks without leaving the game.",
			"<a href=\"#dyes\">Dyes</a>\n<a href=\"#infusions\">Infusions</a>",
			body,
			filterJs);
	}

	void ParseIdList(const char* csv, std::vector<int>& out, size_t maxN)
	{
		if (!csv || !csv[0])
			return;
		const char* p = csv;
		while (*p && out.size() < maxN)
		{
			while (*p == ' ' || *p == ',' || *p == ';' || *p == '\t')
				++p;
			if (!*p)
				break;
			int v = 0;
			bool any = false;
			while (*p >= '0' && *p <= '9')
			{
				any = true;
				v = v * 10 + (*p - '0');
				++p;
			}
			if (any && v > 0)
			{
				bool dup = false;
				for (int x : out)
				{
					if (x == v) { dup = true; break; }
				}
				if (!dup)
					out.push_back(v);
			}
			while (*p && *p != ',' && *p != ';' && *p != ' ' && *p != '\t')
				++p;
		}
	}

	std::string IdsQuery(const std::vector<int>& ids, size_t from, size_t count)
	{
		std::string q;
		for (size_t i = 0; i < count && from + i < ids.size(); ++i)
		{
			if (i) q += ',';
			q += std::to_string(ids[from + i]);
		}
		return q;
	}

	struct PriceRow
	{
		int id = 0;
		std::string name;
		long long buy = 0;
		long long sell = 0;
		bool custom = false;
	};

	void FetchItemNames(const std::vector<int>& ids, std::vector<PriceRow>& rows, int timeoutMs)
	{
		if (ids.empty())
			return;
		const size_t batches = (ids.size() + 199) / 200;
		std::vector<std::string> paths(batches);
		std::vector<Gw2Http::Result> results(batches);
		std::vector<ParallelApiJob> jobs(batches);
		for (size_t bi = 0; bi < batches; ++bi)
		{
			const size_t off = bi * 200;
			const size_t n = (ids.size() - off > 200) ? 200 : (ids.size() - off);
			paths[bi] = "/v2/items?ids=";
			paths[bi] += IdsQuery(ids, off, n);
			jobs[bi].path = paths[bi].c_str();
			jobs[bi].bearer = nullptr;
			jobs[bi].timeoutMs = timeoutMs;
			jobs[bi].out = &results[bi];
		}
		RunParallelApis(jobs.data(), jobs.size());

		for (size_t bi = 0; bi < batches; ++bi)
		{
			const Gw2Http::Result& r = results[bi];
			if (!r.ok)
				continue;
			size_t p = 0;
			while (p < r.body.size())
			{
				size_t brace = r.body.find('{', p);
				if (brace == std::string::npos)
					break;
				size_t end = JsonObjectEnd(r.body, brace);
				if (end == std::string::npos)
					break;
				long long id = JsonIntAfterKey(r.body, "id", brace);
				std::string name = JsonStringAfterKey(r.body, "name", brace);
				if (id > 0 && !name.empty())
				{
					for (PriceRow& row : rows)
					{
						if (row.id == static_cast<int>(id))
						{
							row.name = name;
							break;
						}
					}
				}
				p = end + 1;
			}
		}
	}

	void ApplyNamesFromJson(const std::string& json, std::vector<PriceRow>& rows)
	{
		size_t p = 0;
		while (p < json.size())
		{
			size_t brace = json.find('{', p);
			if (brace == std::string::npos)
				break;
			size_t end = JsonObjectEnd(json, brace);
			if (end == std::string::npos)
				break;
			long long id = JsonIntAfterKey(json, "id", brace);
			std::string name = JsonStringAfterKey(json, "name", brace);
			if (id > 0 && !name.empty())
			{
				for (PriceRow& row : rows)
				{
					if (row.id == static_cast<int>(id))
					{
						row.name = name;
						break;
					}
				}
			}
			p = end + 1;
		}
	}

	void EnsureArmoryNames(const std::wstring& addonDir,
		const std::vector<int>& ids, std::vector<PriceRow>& rows)
	{
		if (ids.empty())
			return;
		const std::wstring path = StemPath(addonDir, "live-armory-names", L".json");
		if (FileFresh(path, kArmoryTtlSec))
		{
			std::string cached = ReadUtf8File(path);
			if (!cached.empty())
			{
				ApplyNamesFromJson(cached, rows);
				int named = 0;
				for (const PriceRow& r : rows)
					if (!r.name.empty()) ++named;
				if (named > static_cast<int>(ids.size()) / 2)
					return;
			}
		}
		FetchItemNames(ids, rows, kLiveBulkTimeoutMs);
		std::string out = "[";
		bool first = true;
		for (const PriceRow& r : rows)
		{
			if (r.name.empty())
				continue;
			if (!first) out += ',';
			first = false;
			out += "{\"id\":";
			out += std::to_string(r.id);
			out += ",\"name\":\"";
			for (char c : r.name)
			{
				if (c == '"' || c == '\\') out += '\\';
				out += c;
			}
			out += "\"}";
		}
		out += ']';
		if (!first)
			WriteUtf8File(path, out);
	}

	bool WatchlistContains(const std::vector<int>& ids, int id)
	{
		for (int x : ids)
			if (x == id) return true;
		return false;
	}

	void SerializeTpWatchIds(const std::vector<int>& ids)
	{
		std::string s;
		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (i) s += ',';
			s += std::to_string(ids[i]);
		}
		if (s.size() >= sizeof(G::TpWatchIds))
			s.resize(sizeof(G::TpWatchIds) - 1);
		std::snprintf(G::TpWatchIds, sizeof(G::TpWatchIds), "%s", s.c_str());
	}

	bool MutateTpWatchlist(const char* op, int id)
	{
		if (id <= 0 || !op || !op[0])
			return false;
		std::vector<int> ids;
		ParseIdList(G::TpWatchIds, ids, 120);
		if (std::strcmp(op, "add") == 0)
		{
			if (WatchlistContains(ids, id))
				return false;
			if (ids.size() >= 120)
				return false;
			ids.push_back(id);
		}
		else if (std::strcmp(op, "remove") == 0)
		{
			bool found = false;
			std::vector<int> next;
			next.reserve(ids.size());
			for (int x : ids)
			{
				if (x == id) { found = true; continue; }
				next.push_back(x);
			}
			if (!found)
				return false;
			ids.swap(next);
		}
		else
			return false;
		SerializeTpWatchIds(ids);
		Settings::SetDirty();
		LivePanels::InvalidateTpCache(AddonPaths::DataDir());
		return true;
	}

	/* Helper writes live-tp-cmd.txt (add/remove); DLL applies on Tick. */
	bool ProcessTpWatchCmdFile(const std::wstring& addonDir)
	{
		const std::wstring path = addonDir + L"\\live-tp-cmd.txt";
		const std::string raw = ReadUtf8File(path);
		if (raw.empty())
			return false;
		DeleteFileW(path.c_str());
		bool changed = false;
		size_t i = 0;
		while (i < raw.size())
		{
			while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\r' || raw[i] == '\n' || raw[i] == '\t'))
				++i;
			if (i >= raw.size())
				break;
			const size_t lineStart = i;
			while (i < raw.size() && raw[i] != '\n' && raw[i] != '\r')
				++i;
			std::string line = raw.substr(lineStart, i - lineStart);
			const char* op = nullptr;
			const char* num = nullptr;
			if (line.rfind("add ", 0) == 0)
			{
				op = "add";
				num = line.c_str() + 4;
			}
			else if (line.rfind("remove ", 0) == 0)
			{
				op = "remove";
				num = line.c_str() + 7;
			}
			if (!op || !num)
				continue;
			int id = 0;
			for (const char* p = num; *p >= '0' && *p <= '9'; ++p)
				id = id * 10 + (*p - '0');
			if (MutateTpWatchlist(op, id))
				changed = true;
		}
		return changed;
	}

	bool ParseTpWatchMutateUrl(const std::string& url, const char** opOut, int* idOut)
	{
		if (url.rfind("about:live-tp-add-", 0) == 0)
		{
			*opOut = "add";
			*idOut = 0;
			for (const char* p = url.c_str() + 18; *p >= '0' && *p <= '9'; ++p)
				*idOut = *idOut * 10 + (*p - '0');
			return *idOut > 0;
		}
		if (url.rfind("about:live-tp-remove-", 0) == 0)
		{
			*opOut = "remove";
			*idOut = 0;
			for (const char* p = url.c_str() + 21; *p >= '0' && *p <= '9'; ++p)
				*idOut = *idOut * 10 + (*p - '0');
			return *idOut > 0;
		}
		return false;
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

	std::string UrlEncodePathSegment(const std::string& s)
	{
		std::string o;
		o.reserve(s.size() * 3);
		for (unsigned char c : s)
		{
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
				c == '-' || c == '_' || c == '.' || c == '~')
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

	void ParseArmoryCatalog(const std::string& body,
		std::vector<int>& armoryIds, std::vector<int>& maxCounts)
	{
		armoryIds.clear();
		maxCounts.clear();
		if (body.empty())
			return;

		/* Prefer objects from ?ids=all: [{"id":1,"max_count":2}, ...] */
		size_t p = 0;
		while (p < body.size() && armoryIds.size() < 400)
		{
			size_t brace = body.find('{', p);
			if (brace == std::string::npos)
				break;
			size_t end = JsonObjectEnd(body, brace);
			if (end == std::string::npos)
				break;
			long long id = JsonIntAfterKey(body, "id", brace);
			long long mx = JsonIntAfterKey(body, "max_count", brace);
			if (id > 0)
			{
				armoryIds.push_back(static_cast<int>(id));
				maxCounts.push_back(mx > 0 ? static_cast<int>(mx) : 1);
			}
			p = end + 1;
		}
		if (!armoryIds.empty())
			return;

		/* Root /v2/legendaryarmory is a bare id list: [105317, 83162, ...] */
		p = 0;
		while (p < body.size() && armoryIds.size() < 400)
		{
			while (p < body.size() && (body[p] < '0' || body[p] > '9'))
				++p;
			if (p >= body.size())
				break;
			int v = 0;
			bool any = false;
			while (p < body.size() && body[p] >= '0' && body[p] <= '9')
			{
				any = true;
				v = v * 10 + (body[p] - '0');
				++p;
			}
			if (any && v > 0)
			{
				armoryIds.push_back(v);
				maxCounts.push_back(1);
			}
		}
	}

	std::string EnsureArmoryCatalogJson(const std::wstring& addonDir)
	{
		const std::wstring path = StemPath(addonDir, "live-armory", L".json");
		if (FileFresh(path, kArmoryTtlSec))
		{
			std::string cached = ReadUtf8File(path);
			if (!cached.empty() && cached.find('{') != std::string::npos)
				return cached;
		}
		/* Bare /v2/legendaryarmory returns ids only — need ids=all for max_count. */
		auto r = Gw2Http::Api("/v2/legendaryarmory?ids=all", nullptr, kLiveBulkTimeoutMs);
		if (r.ok && r.body.find("\"id\"") != std::string::npos)
		{
			WriteUtf8File(path, r.body);
			return r.body;
		}
		std::string stale = ReadUtf8File(path);
		if (!stale.empty())
			return stale;
		return r.ok ? r.body : std::string{};
	}

	std::string BuildProgressHtml(const std::wstring& addonDir, const char* apiKey)
	{
		std::string body;
		const bool hasKey = apiKey && apiKey[0];

		std::vector<int> armoryIds;
		std::vector<int> maxCounts;
		const std::string catalogBody = EnsureArmoryCatalogJson(addonDir);
		ParseArmoryCatalog(catalogBody, armoryIds, maxCounts);
		std::string catalogErr;
		if (armoryIds.empty())
			catalogErr = catalogBody.empty() ? "empty response" : "parse failed";

		std::vector<PriceRow> nameRows;
		nameRows.reserve(armoryIds.size());
		for (int id : armoryIds)
		{
			PriceRow r;
			r.id = id;
			nameRows.push_back(r);
		}
		if (!armoryIds.empty())
			EnsureArmoryNames(addonDir, armoryIds, nameRows);

		std::vector<int> ownedCount(armoryIds.size(), -1); /* -1 = unknown */
		bool accountOk = false;
		bool accountDenied = false;
		Gw2Http::Result acc;
		Gw2Http::Result chars;
		bool hitAcc = false;
		bool hitChars = false;
		if (hasKey)
		{
			hitAcc = TryCacheHit(addonDir, "live-acc-armory", kAccountTtlSec, acc);
			hitChars = TryCacheHit(addonDir, "live-chars", kAccountTtlSec, chars);

			ParallelApiJob jobs[2];
			size_t nJobs = 0;
			if (!hitAcc)
			{
				jobs[nJobs] = {"/v2/account/legendaryarmory", apiKey, kLiveHttpTimeoutMs, &acc};
				++nJobs;
			}
			if (!hitChars)
			{
				jobs[nJobs] = {"/v2/characters", apiKey, kLiveHttpTimeoutMs, &chars};
				++nJobs;
			}
			RunParallelApis(jobs, nJobs);
			if (!hitAcc)
			{
				StoreCache(addonDir, "live-acc-armory", acc);
				PreferStaleCache(addonDir, "live-acc-armory", acc);
			}
			if (!hitChars)
			{
				StoreCache(addonDir, "live-chars", chars);
				PreferStaleCache(addonDir, "live-chars", chars);
			}

			if (acc.ok)
			{
				accountOk = true;
				for (int& c : ownedCount)
					c = 0;
				size_t p = 0;
				while (p < acc.body.size())
				{
					size_t brace = acc.body.find('{', p);
					if (brace == std::string::npos)
						break;
					size_t end = JsonObjectEnd(acc.body, brace);
					if (end == std::string::npos)
						break;
					long long id = JsonIntAfterKey(acc.body, "id", brace);
					long long cnt = JsonIntAfterKey(acc.body, "count", brace);
					if (id > 0)
					{
						for (size_t i = 0; i < armoryIds.size(); ++i)
						{
							if (armoryIds[i] == static_cast<int>(id))
							{
								ownedCount[i] = cnt > 0 ? static_cast<int>(cnt) : 0;
								break;
							}
						}
					}
					p = end + 1;
				}
			}
			else if (acc.status == 403 || acc.status == 401)
				accountDenied = true;
		}

		if (hasKey && accountOk)
		{
			body += "<div class=\"keybox ok\"><h3>Account legendary armory</h3>"
				"<p>Unlocked counts from your API key — read-only. "
				"Deep links open gw2efficiency / wiki for the rest.</p></div>";
		}
		else if (hasKey && accountDenied)
		{
			body += "<div class=\"keybox warn\"><h3>Need more API scopes</h3>"
				"<p>Legendary progress needs <strong>account</strong> + <strong>inventories</strong> + "
				"<strong>unlocks</strong>. Characters need <strong>characters</strong>. "
				"Vault still uses <strong>progression</strong>. Create a key with those scopes in Options.</p></div>";
		}
		else
		{
			body += "<div class=\"keybox warn\"><h3>Public catalog</h3>"
				"<p>Showing the legendary armory list. Paste an API key in Nexus Options "
				"(scopes: account, inventories, unlocks, characters) to fill unlocks + roster.</p></div>";
		}

		body += "<section class=\"block\" id=\"armory\"><div class=\"head\"><h2>Legendary Armory</h2>"
			"<p>";
		body += accountOk ? "Your unlocks + public catalog" : "Public catalog — tick locally if you want";
		body += "</p></div><div class=\"body\"><ul class=\"checks\">";

		int listed = 0;
		int unlocked = 0;
		for (size_t i = 0; i < armoryIds.size(); ++i)
		{
			const std::string& name = nameRows[i].name;
			const bool have = ownedCount[i] > 0;
			if (have) ++unlocked;
			body += "<li><label class=\"check\"><input type=\"checkbox\"";
			if (have) body += " checked";
			body += "/><span class=\"box\" aria-hidden=\"true\"></span><span class=\"txt\"><strong>";
			body += HtmlEscape(name.empty() ? ("Item #" + std::to_string(armoryIds[i])) : name);
			body += "</strong>";
			if (accountOk)
			{
				body += " <span class=\"muted\">";
				body += std::to_string(ownedCount[i] < 0 ? 0 : ownedCount[i]);
				body += "/";
				body += std::to_string(maxCounts[i]);
				body += "</span>";
			}
			body += " — <a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Special:Search/";
			body += std::to_string(armoryIds[i]);
			body += "\">wiki</a></span></label></li>";
			++listed;
		}
		body += "</ul>";
		if (listed == 0)
		{
			body += "<p class=\"note\">Could not load legendary armory catalog";
			if (!catalogErr.empty())
			{
				body += " (";
				body += HtmlEscape(catalogErr);
				body += ")";
			}
			body += ". Hit <strong>Reload</strong> — catalog is cached for a day after the first success.</p>";
		}
		else if (accountOk)
		{
			body += "<p class=\"meta\">";
			body += std::to_string(unlocked);
			body += " / ";
			body += std::to_string(listed);
			body += " unlocked in armory.</p>";
		}
		body += "<p style=\"margin-top:12px\">"
			"<a class=\"link\" href=\"https://gw2efficiency.com/account/legendaries\">gw2efficiency — Legendaries</a>"
			" · <a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Legendary_Armory\">Wiki — Legendary Armory</a>"
			"</p></div></section>\n";

		/* Characters — roster + one bulk details call (not N sequential). */
		body += "<section class=\"block\" id=\"chars\"><div class=\"head\"><h2>Characters</h2>"
			"<p>Roster from API (optional key)</p></div><div class=\"body\">";
		if (hasKey)
		{
			if (chars.ok)
			{
				std::vector<std::string> names;
				size_t i = 0;
				while (i < chars.body.size() && names.size() < 64)
				{
					size_t q = chars.body.find('"', i);
					if (q == std::string::npos)
						break;
					size_t after = q;
					std::string val = ReadJsonQuoted(chars.body, q, &after);
					i = after;
					if (val.empty())
						continue;
					names.push_back(val);
				}

				struct CharRow { std::string name; std::string profession; long long level = -1; };
				std::vector<CharRow> charRows;
				charRows.reserve(names.size());
				for (const std::string& nm : names)
				{
					CharRow cr;
					cr.name = nm;
					charRows.push_back(std::move(cr));
				}

				constexpr size_t kMaxCharDetails = 24;
				const size_t detailN = charRows.size() < kMaxCharDetails
					? charRows.size() : kMaxCharDetails;
				if (detailN > 0)
				{
					Gw2Http::Result detail;
					const bool hitDetail = TryCacheHit(addonDir, "live-chars-detail",
						kAccountTtlSec, detail);
					if (!hitDetail)
					{
						std::string path = "/v2/characters?ids=";
						for (size_t ci = 0; ci < detailN; ++ci)
						{
							if (ci) path += ',';
							path += UrlEncodePathSegment(charRows[ci].name);
						}
						detail = Gw2Http::Api(path.c_str(), apiKey, kLiveBulkTimeoutMs);
						StoreCache(addonDir, "live-chars-detail", detail);
						PreferStaleCache(addonDir, "live-chars-detail", detail);
					}
					if (detail.ok)
					{
						size_t p = 0;
						while (p < detail.body.size())
						{
							size_t brace = detail.body.find('{', p);
							if (brace == std::string::npos)
								break;
							size_t end = JsonObjectEnd(detail.body, brace);
							if (end == std::string::npos)
								break;
							std::string nm = JsonStringAfterKey(detail.body, "name", brace);
							std::string profession = JsonStringAfterKey(detail.body, "profession", brace);
							long long level = JsonIntAfterKey(detail.body, "level", brace);
							if (!nm.empty())
							{
								for (CharRow& cr : charRows)
								{
									if (cr.name == nm)
									{
										cr.profession = profession;
										cr.level = level;
										break;
									}
								}
							}
							p = end + 1;
						}
					}
				}

				body += "<ul class=\"rows\">";
				for (size_t ci = 0; ci < detailN; ++ci)
				{
					const CharRow& cr = charRows[ci];
					body += "<li><span class=\"t\">";
					body += HtmlEscape(cr.name);
					body += "</span><span class=\"s\">";
					if (cr.level >= 0)
					{
						body += "Level ";
						body += std::to_string(cr.level);
					}
					if (!cr.profession.empty())
					{
						if (cr.level >= 0) body += " · ";
						body += HtmlEscape(cr.profession);
					}
					body += " · <a class=\"link\" href=\"?gw2igh-newtab="
						"https%3A%2F%2Fgw2efficiency.com%2Faccount%2Foverview\">gw2efficiency</a>"
						"</span></li>";
				}
				body += "</ul>";
				if (names.size() > detailN)
				{
					body += "<p class=\"meta\">Showing ";
					body += std::to_string(detailN);
					body += " of ";
					body += std::to_string(names.size());
					body += " characters.</p>";
				}
			}
			else if (chars.status == 403 || chars.status == 401)
			{
				body += "<p class=\"note\">Character roster needs the <strong>characters</strong> scope on your API key.</p>";
			}
			else
			{
				body += "<p class=\"note\">Could not load characters";
				if (!chars.error.empty())
				{
					body += " (";
					body += HtmlEscape(chars.error);
					body += ")";
				}
				body += ".</p>";
			}
		}
		else
		{
			body += "<p class=\"note\">Add an API key with the <strong>characters</strong> scope to list your roster here.</p>";
		}
		body += "<p style=\"margin-top:12px\">"
			"<a class=\"link\" href=\"https://gw2efficiency.com/\">gw2efficiency</a>"
			" · <a class=\"link\" href=\"https://account.arena.net/applications\">Manage API keys</a>"
			"</p></div></section>\n";

		return BuildPage(
			"Live — Legendaries &amp; Characters",
			"GW2 In-Game Helper · Live",
			"Legendaries &amp; Characters",
			"Armory progress and character roster — API fills + deep links, no game memory.",
			"<a href=\"#armory\">Armory</a>\n<a href=\"#chars\">Characters</a>",
			body);
	}

	std::string OfflineShellHtml(const char* title, const char* heading, const char* note)
	{
		std::string body = "<section class=\"block\"><div class=\"head\"><h2>Loading Live data…</h2></div><div class=\"body\">";
		body += "<p class=\"note\">";
		body += note;
		body += "</p>";
		body += "<ul class=\"rows\">";
		body += "<li><a class=\"link\" href=\"about:daily-weekly\">Open offline Daily / Weekly checklist</a></li>";
		body += "<li><a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Wizard%27s_Vault/Easy_objectives\">Wiki — Easy Vault objectives</a></li>";
		body += "<li><a class=\"link\" href=\"https://gw2timer.com/\">GW2Timer</a></li>";
		body += "</ul></div></section>\n";
		return BuildPage(title, "GW2 In-Game Helper · Live", heading,
			"Loading in the background so the game stays smooth…",
			nullptr, body);
	}

	bool VerMatches(const std::wstring& verPath)
	{
		std::string v = ReadUtf8File(verPath);
		while (!v.empty() && (v.back() == '\n' || v.back() == '\r' || v.back() == ' '))
			v.pop_back();
		return v == kPanelVer;
	}

	bool PanelReady(const std::wstring& addonDir, const char* stem)
	{
		const std::wstring okPath = StemPath(addonDir, stem, L".ok");
		return GetFileAttributesW(okPath.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	struct LiveAsyncJob
	{
		std::wstring addonDir;
		std::string stem;
		std::string apiKey;
		std::string tpWatchIds;
		unsigned generation = 0;
		enum Kind { Dailies, News, Fashion, Tp, Progress } kind = Dailies;
	};

	struct LiveReadyNav
	{
		std::string stem;
		std::string fileUrl;
	};

	struct LiveAsyncState
	{
		std::mutex mu;
		unsigned generation = 1;
		std::vector<HANDLE> joinable; /* finished threads awaiting CloseHandle */
		std::vector<std::string> runningStems;
		std::deque<LiveAsyncJob*> queue;
		std::vector<LiveReadyNav> readyNav;
	};

	LiveAsyncState gAsync;

	bool StemIsRunningOrQueued(const std::string& stem)
	{
		for (const std::string& s : gAsync.runningStems)
			if (s == stem) return true;
		for (LiveAsyncJob* j : gAsync.queue)
			if (j && j->stem == stem) return true;
		return false;
	}

	void PumpLiveQueueUnlocked(); /* defined after LiveWorkerProc */

	DWORD WINAPI LiveWorkerProc(void* param)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
		LiveAsyncJob* job = static_cast<LiveAsyncJob*>(param);
		std::string html;
		if (job->kind == LiveAsyncJob::Dailies)
			html = BuildDailiesHtml(job->addonDir, job->apiKey.c_str());
		else if (job->kind == LiveAsyncJob::News)
			html = BuildNewsHtml();
		else if (job->kind == LiveAsyncJob::Fashion)
			html = BuildFashionHtml(job->addonDir);
		else if (job->kind == LiveAsyncJob::Tp)
			html = BuildTpHtml(job->tpWatchIds.c_str(), true);
		else
			html = BuildProgressHtml(job->addonDir, job->apiKey.c_str());

		/* Write on the worker — never dump multi-KB HTML on the game/UI thread. */
		std::string fileUrl;
		bool accept = false;
		{
			std::lock_guard<std::mutex> lock(gAsync.mu);
			accept = (job->generation == gAsync.generation && !html.empty());
		}
		if (accept)
		{
			const std::wstring htmlPath = StemPath(job->addonDir, job->stem.c_str(), L".html");
			const std::wstring verPath = StemPath(job->addonDir, job->stem.c_str(), L".ver");
			const std::wstring okPath = StemPath(job->addonDir, job->stem.c_str(), L".ok");
			if (WriteUtf8File(htmlPath, html))
			{
				WriteUtf8File(verPath, kPanelVer);
				WriteUtf8File(okPath, "1");
				fileUrl = PathToFileUrl(htmlPath);
			}
		}

		{
			std::lock_guard<std::mutex> lock(gAsync.mu);
			if (!fileUrl.empty())
			{
				LiveReadyNav nav;
				nav.stem = job->stem;
				nav.fileUrl = std::move(fileUrl);
				gAsync.readyNav.push_back(std::move(nav));
			}
			for (size_t i = 0; i < gAsync.runningStems.size(); ++i)
			{
				if (gAsync.runningStems[i] == job->stem)
				{
					gAsync.runningStems.erase(gAsync.runningStems.begin() +
						static_cast<std::ptrdiff_t>(i));
					break;
				}
			}
			PumpLiveQueueUnlocked();
		}
		delete job;
		return 0;
	}

	void PumpLiveQueueUnlocked()
	{
		while (static_cast<int>(gAsync.runningStems.size()) < kMaxLiveWorkers &&
			!gAsync.queue.empty())
		{
			LiveAsyncJob* job = gAsync.queue.front();
			gAsync.queue.pop_front();
			if (!job)
				continue;
			if (job->generation != gAsync.generation)
			{
				delete job;
				continue;
			}
			gAsync.runningStems.push_back(job->stem);
			HANDLE th = CreateThread(nullptr, 0, LiveWorkerProc, job, 0, nullptr);
			if (!th)
			{
				gAsync.runningStems.pop_back();
				delete job;
				continue;
			}
			gAsync.joinable.push_back(th);
		}
	}

	void ReapJoinableUnlocked()
	{
		std::vector<HANDLE> keep;
		keep.reserve(gAsync.joinable.size());
		for (HANDLE th : gAsync.joinable)
		{
			if (!th)
				continue;
			if (WaitForSingleObject(th, 0) == WAIT_OBJECT_0)
				CloseHandle(th);
			else
				keep.push_back(th);
		}
		gAsync.joinable.swap(keep);
	}

	void StartLiveWorker(const std::wstring& addonDir, const char* stem, LiveAsyncJob::Kind kind)
	{
		if (!stem || !stem[0])
			return;
		std::lock_guard<std::mutex> lock(gAsync.mu);
		ReapJoinableUnlocked();
		if (StemIsRunningOrQueued(stem))
			return;

		auto* job = new LiveAsyncJob();
		job->addonDir = addonDir;
		job->stem = stem;
		job->apiKey = G::Gw2ApiKey;
		job->tpWatchIds = G::TpWatchIds;
		job->generation = gAsync.generation;
		job->kind = kind;
		gAsync.queue.push_back(job);
		PumpLiveQueueUnlocked();
	}

	std::string EnsurePanel(const std::wstring& addonDir, const char* stem,
		LiveAsyncJob::Kind kind, const char* offlineTitle, const char* offlineHeading)
	{
		const std::wstring path = StemPath(addonDir, stem, L".html");
		const std::wstring verPath = StemPath(addonDir, stem, L".ver");
		const DWORD ttl = (kind == LiveAsyncJob::Tp) ? kTpHtmlTtlSec : kHtmlTtlSec;
		if (VerMatches(verPath) && FileFresh(path, ttl) && PanelReady(addonDir, stem))
			return PathToFileUrl(path);

		/* TP tip page — no network; ImGui TpWatchPad owns the real watchlist. */
		if (kind == LiveAsyncJob::Tp)
		{
			WriteUtf8File(path, BuildTpHtml(nullptr, false));
			WriteUtf8File(verPath, kPanelVer);
			WriteUtf8File(StemPath(addonDir, stem, L".ok"), "1");
			return PathToFileUrl(path);
		}

		const std::string shell = OfflineShellHtml(offlineTitle, offlineHeading,
			"Fetching Live data in the background. This page will refresh when ready. "
			"You can keep playing — the game should not freeze.");
		WriteUtf8File(path, shell);
		DeleteFileW(StemPath(addonDir, stem, L".ok").c_str());

		StartLiveWorker(addonDir, stem, kind);
		return PathToFileUrl(path);
	}
}

bool LivePanels::IsLiveAbout(const char* url)
{
	if (!url)
		return false;
	if (std::strncmp(url, "about:live-tp-add-", 18) == 0 ||
		std::strncmp(url, "about:live-tp-remove-", 21) == 0)
		return true;
	return std::strcmp(url, "about:live-dailies") == 0 ||
		std::strcmp(url, "about:live-news") == 0 ||
		std::strcmp(url, "about:live-fashion") == 0 ||
		std::strcmp(url, "about:live-tp") == 0 ||
		std::strcmp(url, "about:live-progress") == 0;
}

bool LivePanels::IsLiveUrl(const char* url)
{
	if (!url || !url[0])
		return false;
	if (IsLiveAbout(url))
		return true;
	return std::strstr(url, "live-dailies.html") != nullptr ||
		std::strstr(url, "live-news.html") != nullptr ||
		std::strstr(url, "live-fashion.html") != nullptr ||
		std::strstr(url, "live-tp.html") != nullptr ||
		std::strstr(url, "live-progress.html") != nullptr;
}

std::string LivePanels::ResolveAboutUrl(const std::wstring& addonDir, const std::string& url)
{
	if (addonDir.empty() || url.empty())
		return {};
	const char* op = nullptr;
	int id = 0;
	if (ParseTpWatchMutateUrl(url, &op, &id))
	{
		MutateTpWatchlist(op, id); /* InvalidateTpCache inside */
		return EnsurePanel(addonDir, "live-tp", LiveAsyncJob::Tp,
			"Live — Trading Post Watchlist", "My TP Watchlist");
	}
	if (url == "about:live-dailies")
		return EnsurePanel(addonDir, "live-dailies", LiveAsyncJob::Dailies,
			"Live — Dailies &amp; Vault", "Dailies &amp; Wizard’s Vault");
	if (url == "about:live-news")
		return EnsurePanel(addonDir, "live-news", LiveAsyncJob::News,
			"Live — News &amp; Patch Digest", "News &amp; Patch Digest");
	if (url == "about:live-fashion")
		return EnsurePanel(addonDir, "live-fashion", LiveAsyncJob::Fashion,
			"Live — Fashion Wishlist", "Fashion Wishlist");
	if (url == "about:live-tp")
		return EnsurePanel(addonDir, "live-tp", LiveAsyncJob::Tp,
			"Live — Trading Post Watchlist", "My TP Watchlist");
	if (url == "about:live-progress")
		return EnsurePanel(addonDir, "live-progress", LiveAsyncJob::Progress,
			"Live — Legendaries &amp; Characters", "Legendaries &amp; Characters");
	return {};
}

void LivePanels::Tick()
{
	/* Legacy CEF helper cmd file (TP is ImGui now) — keep cheap no-op if absent. */
	{
		const std::wstring dir = AddonPaths::DataDir();
		if (!dir.empty())
			ProcessTpWatchCmdFile(dir);
	}

	std::vector<LiveReadyNav> ready;
	{
		std::lock_guard<std::mutex> lock(gAsync.mu);
		ReapJoinableUnlocked();
		ready.swap(gAsync.readyNav);
	}
	if (ready.empty())
		return;

	/* Navigate only — HTML already on disk from the worker. */
	const char* cur = WikiBrowser::CurrentUrlCStr();
	if (!cur)
		return;
	for (const LiveReadyNav& nav : ready)
	{
		if (nav.fileUrl.empty() || nav.stem.empty())
			continue;
		const bool onPanel =
			std::strstr(cur, (nav.stem + ".html").c_str()) != nullptr ||
			(nav.stem == "live-dailies" && std::strstr(cur, "about:live-dailies")) ||
			(nav.stem == "live-news" && std::strstr(cur, "about:live-news")) ||
			(nav.stem == "live-fashion" && std::strstr(cur, "about:live-fashion")) ||
			(nav.stem == "live-tp" && std::strstr(cur, "about:live-tp")) ||
			(nav.stem == "live-progress" && std::strstr(cur, "about:live-progress"));
		if (onPanel)
			WikiBrowser::Navigate(nav.fileUrl);
	}
}

void LivePanels::InvalidateTpCache(const std::wstring& addonDir)
{
	{
		std::lock_guard<std::mutex> lock(gAsync.mu);
		++gAsync.generation;
		for (LiveAsyncJob* j : gAsync.queue)
			delete j;
		gAsync.queue.clear();
		gAsync.readyNav.clear();
		/* Allow a new job for the same stem; old workers discard via generation. */
		gAsync.runningStems.clear();
	}
	if (addonDir.empty())
		return;
	DeleteFileW(StemPath(addonDir, "live-tp", L".ver").c_str());
	DeleteFileW(StemPath(addonDir, "live-tp", L".ok").c_str());
}

void LivePanels::InvalidateCaches(const std::wstring& addonDir)
{
	{
		std::lock_guard<std::mutex> lock(gAsync.mu);
		++gAsync.generation;
		for (LiveAsyncJob* j : gAsync.queue)
			delete j;
		gAsync.queue.clear();
		gAsync.readyNav.clear();
		gAsync.runningStems.clear();
	}
	if (addonDir.empty())
		return;
	const char* stems[] = {
		"live-dailies", "live-news", "live-fashion", "live-tp", "live-progress",
		"live-colors", "live-armory", "live-armory-names",
		"live-season", "live-craft", "live-bosses", "live-vault-obj",
		"live-vault-daily", "live-vault-weekly", "live-vault-special",
		"live-acc-armory", "live-chars", "live-chars-detail"
	};
	for (const char* stem : stems)
	{
		DeleteFileW(StemPath(addonDir, stem, L".html").c_str());
		DeleteFileW(StemPath(addonDir, stem, L".ver").c_str());
		DeleteFileW(StemPath(addonDir, stem, L".ok").c_str());
		DeleteFileW(StemPath(addonDir, stem, L".json").c_str());
	}
}

void LivePanels::Shutdown()
{
	std::vector<HANDLE> wait;
	{
		std::lock_guard<std::mutex> lock(gAsync.mu);
		++gAsync.generation;
		for (LiveAsyncJob* j : gAsync.queue)
			delete j;
		gAsync.queue.clear();
		gAsync.readyNav.clear();
		gAsync.runningStems.clear();
		wait.swap(gAsync.joinable);
	}
	/* Bounded join — never hang Nexus unload on a stuck WinHTTP call. */
	for (HANDLE th : wait)
	{
		if (!th)
			continue;
		WaitForSingleObject(th, 500);
		CloseHandle(th);
	}
}
