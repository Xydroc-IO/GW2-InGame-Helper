#include "VaultPadInternal.h"

#include "JsonView.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <windows.h>

namespace VaultDetail
{
	size_t JsonObjectEnd(const std::string& json, size_t openBrace)
	{
		return JsonView::ObjectEnd(json, openBrace);
	}

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

	std::string FormatIsoDateUtc(const std::string& iso)
	{
		int y = 0, mo = 0, d = 0;
		if (std::sscanf(iso.c_str(), "%d-%d-%d", &y, &mo, &d) != 3 ||
			y < 2000 || mo < 1 || mo > 12 || d < 1 || d > 31)
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
		*out = _mkgmtime(&tm);
		return *out != (time_t)-1;
	}

	std::string SeasonBlurb(const std::string& /*startIso*/, const std::string& endIso)
	{
		std::string out;
		time_t endT = 0;
		const bool hasEnd = !endIso.empty() && ParseIsoUtc(endIso, &endT);
		const time_t now = time(nullptr);
		if (hasEnd)
		{
			out = "Ends ";
			out += FormatIsoDateUtc(endIso);
			const double days = difftime(endT, now) / 86400.0;
			if (days >= 0)
			{
				const int n = static_cast<int>(days + 0.999);
				out += " | ";
				out += std::to_string(n);
				out += (n == 1 ? " day left" : " days left");
			}
			else
				out += " | ended";
		}
		if (out.empty())
			out = "Season dates unavailable.";
		return out;
	}

	/* GW2 daily = 00:00 UTC. Weekly = Monday 07:30 UTC. */
	std::string FormatCountdown(long long sec)
	{
		if (sec < 0) sec = 0;
		const long long d = sec / 86400;
		const long long h = (sec % 86400) / 3600;
		const long long m = (sec % 3600) / 60;
		const long long s = sec % 60;
		char buf[64];
		if (d > 0)
			std::snprintf(buf, sizeof(buf), "%lldd %lldh %lldm", d, h, m);
		else if (h > 0)
			std::snprintf(buf, sizeof(buf), "%lldh %lldm", h, m);
		else
			std::snprintf(buf, sizeof(buf), "%lldm %llds", m, s);
		return buf;
	}

	void UtcNowParts(int& y, int& mo, int& d, int& h, int& mi, int& s, int& wday)
	{
		const time_t now = time(nullptr);
		struct tm tm{};
#ifdef _WIN32
		gmtime_s(&tm, &now);
#else
		gmtime_r(&now, &tm);
#endif
		y = tm.tm_year + 1900;
		mo = tm.tm_mon + 1;
		d = tm.tm_mday;
		h = tm.tm_hour;
		mi = tm.tm_min;
		s = tm.tm_sec;
		wday = tm.tm_wday; /* 0=Sun ... 6=Sat */
	}

	time_t MakeUtc(int y, int mo, int d, int h, int mi, int s)
	{
		struct tm tm{};
		tm.tm_year = y - 1900;
		tm.tm_mon = mo - 1;
		tm.tm_mday = d;
		tm.tm_hour = h;
		tm.tm_min = mi;
		tm.tm_sec = s;
		return _mkgmtime(&tm);
	}

	void AddUtcDays(int& y, int& mo, int& d, int days)
	{
		const time_t t = MakeUtc(y, mo, d, 12, 0, 0) + static_cast<time_t>(days) * 86400;
		struct tm tm{};
#ifdef _WIN32
		gmtime_s(&tm, &t);
#else
		gmtime_r(&t, &tm);
#endif
		y = tm.tm_year + 1900;
		mo = tm.tm_mon + 1;
		d = tm.tm_mday;
	}

	long long SecUntilDailyResetUtc()
	{
		int y, mo, d, h, mi, s, wday;
		UtcNowParts(y, mo, d, h, mi, s, wday);
		(void)wday;
		time_t next = MakeUtc(y, mo, d, 0, 0, 0);
		const time_t now = time(nullptr);
		if (next <= now)
		{
			AddUtcDays(y, mo, d, 1);
			next = MakeUtc(y, mo, d, 0, 0, 0);
		}
		return static_cast<long long>(difftime(next, now));
	}

	long long SecUntilWeeklyResetUtc()
	{
		/* Monday 07:30 UTC. tm_wday: 0=Sun, 1=Mon, ... */
		int y, mo, d, h, mi, s, wday;
		UtcNowParts(y, mo, d, h, mi, s, wday);
		const time_t now = time(nullptr);
		int daysAhead = (1 - wday + 7) % 7; /* days until Monday */
		int ty = y, tmo = mo, td = d;
		AddUtcDays(ty, tmo, td, daysAhead);
		time_t next = MakeUtc(ty, tmo, td, 7, 30, 0);
		if (next <= now)
		{
			AddUtcDays(ty, tmo, td, 7);
			next = MakeUtc(ty, tmo, td, 7, 30, 0);
		}
		return static_cast<long long>(difftime(next, now));
	}
	void ParseVaultObjs(const std::string& json, std::vector<Obj>& out, int maxItems,
		int maxAcclaim)
	{
		out.clear();
		size_t pos = json.find('[');
		if (pos == std::string::npos) return;
		size_t i = pos;
		while (static_cast<int>(out.size()) < maxItems)
		{
			size_t obj = json.find('{', i);
			if (obj == std::string::npos) break;
			size_t end = JsonObjectEnd(json, obj);
			if (end == std::string::npos) break;
			const std::string chunk = json.substr(obj, end - obj + 1);
			Obj o;
			o.title = JsonStringAfterKey(chunk, "title");
			if (o.title.empty())
				o.title = JsonStringAfterKey(chunk, "name");
			o.track = JsonStringAfterKey(chunk, "track");
			o.cur = static_cast<int>(JsonIntAfterKey(chunk, "progress_current"));
			o.need = static_cast<int>(JsonIntAfterKey(chunk, "progress_complete"));
			o.acclaim = static_cast<int>(JsonIntAfterKey(chunk, "acclaim"));
			if (o.cur < 0) o.cur = 0;
			if (o.need < 0) o.need = 0;
			if (o.acclaim < 0) o.acclaim = 0;
			const bool claimed = JsonBoolAfterKey(chunk, "claimed");
			o.done = claimed || (o.need > 0 && o.cur >= o.need);
			i = end + 1;
			if (o.title.empty()) continue;
			if (maxAcclaim > 0 && (o.acclaim <= 0 || o.acclaim > maxAcclaim))
				continue;
			out.push_back(std::move(o));
		}
	}
} // namespace VaultDetail
