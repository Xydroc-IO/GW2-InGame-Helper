#include "PathingSchedule.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <windows.h>

namespace
{
	struct Field
	{
		bool any = true;
		std::vector<int> values; /* explicit / stepped */
	};

	bool ParsePart(const std::string& part, int minV, int maxV, Field& out)
	{
		out = {};
		if (part.empty() || part == "*")
		{
			out.any = true;
			return true;
		}
		out.any = false;
		/* step: *\/n or a-b/n or list */
		auto addRange = [&](int a, int b, int step) {
			if (step < 1)
				step = 1;
			a = std::max(a, minV);
			b = std::min(b, maxV);
			for (int v = a; v <= b; v += step)
				out.values.push_back(v);
		};

		size_t start = 0;
		while (start < part.size())
		{
			size_t comma = part.find(',', start);
			const std::string tok = part.substr(start,
				comma == std::string::npos ? std::string::npos : comma - start);
			start = comma == std::string::npos ? part.size() : comma + 1;

			int step = 1;
			std::string body = tok;
			const size_t slash = tok.find('/');
			if (slash != std::string::npos)
			{
				body = tok.substr(0, slash);
				step = std::atoi(tok.c_str() + slash + 1);
				if (step < 1)
					step = 1;
			}
			if (body == "*" || body.empty())
			{
				addRange(minV, maxV, step);
				continue;
			}
			const size_t dash = body.find('-');
			if (dash != std::string::npos)
			{
				const int a = std::atoi(body.c_str());
				const int b = std::atoi(body.c_str() + dash + 1);
				addRange(a, b, step);
			}
			else
			{
				const int v = std::atoi(body.c_str());
				addRange(v, v, 1);
			}
		}
		return !out.values.empty() || out.any;
	}

	bool MatchField(const Field& f, int v)
	{
		if (f.any)
			return true;
		for (int x : f.values)
		{
			if (x == v)
				return true;
		}
		return false;
	}

	bool ParseCron(const std::string& cron, Field& minute, Field& hour,
		Field& dom, Field& month, Field& dow)
	{
		std::string s = cron;
		/* collapse whitespace */
		std::string tok;
		std::vector<std::string> parts;
		for (char c : s)
		{
			if (c == ' ' || c == '\t')
			{
				if (!tok.empty())
				{
					parts.push_back(tok);
					tok.clear();
				}
			}
			else
				tok.push_back(c);
		}
		if (!tok.empty())
			parts.push_back(tok);
		if (parts.size() != 5)
			return false;
		return ParsePart(parts[0], 0, 59, minute) &&
			ParsePart(parts[1], 0, 23, hour) &&
			ParsePart(parts[2], 1, 31, dom) &&
			ParsePart(parts[3], 1, 12, month) &&
			ParsePart(parts[4], 0, 6, dow); /* 0=Sun ... 6=Sat (Blish) */
	}

	bool CronMatchesAt(const Field& minute, const Field& hour, const Field& dom,
		const Field& month, const Field& dow, std::int64_t unixUtc)
	{
		std::tm tm{};
		const time_t t = static_cast<time_t>(unixUtc);
#if defined(_WIN32)
		gmtime_s(&tm, &t);
#else
		gmtime_r(&t, &tm);
#endif
		/* tm_wday: 0=Sun - matches Blish */
		return MatchField(minute, tm.tm_min) &&
			MatchField(hour, tm.tm_hour) &&
			MatchField(dom, tm.tm_mday) &&
			MatchField(month, tm.tm_mon + 1) &&
			MatchField(dow, tm.tm_wday);
	}
}

bool PathingSchedule::IsActiveUtc(const std::string& cronFiveField,
	float durationMinutes, std::int64_t nowUnixUtc)
{
	if (cronFiveField.empty())
		return true;
	if (!(durationMinutes > 0.f))
		return true; /* Blish: missing duration -> treat as always / ignore */

	Field minute, hour, dom, month, dow;
	if (!ParseCron(cronFiveField, minute, hour, dom, month, dow))
		return true; /* malformed -> do not hide content */

	const int windowSecs = static_cast<int>(durationMinutes * 60.f) + 59;
	/* Sample each minute in [now-duration, now] for a cron start. */
	for (int back = 0; back <= windowSecs; back += 60)
	{
		const std::int64_t t = nowUnixUtc - back;
		/* Floor to minute */
		const std::int64_t minuteStart = t - (t % 60);
		if (!CronMatchesAt(minute, hour, dom, month, dow, minuteStart))
			continue;
		const std::int64_t end = minuteStart +
			static_cast<std::int64_t>(durationMinutes * 60.f);
		if (nowUnixUtc >= minuteStart && nowUnixUtc < end)
			return true;
	}
	return false;
}

bool PathingSchedule::MarkerActive(const char* schedule, float scheduleDurationMin,
	std::int64_t nowUnixUtc)
{
	if (!schedule || !schedule[0])
		return true;
	return IsActiveUtc(schedule, scheduleDurationMin, nowUnixUtc);
}

std::int64_t PathingSchedule::NowUnixUtc()
{
	FILETIME ft{};
	GetSystemTimeAsFileTime(&ft);
	ULARGE_INTEGER u{};
	u.LowPart = ft.dwLowDateTime;
	u.HighPart = ft.dwHighDateTime;
	/* FILETIME epochs at 1601; Unix at 1970 - 11644473600 seconds. */
	constexpr std::uint64_t kEpochDiff = 11644473600ull;
	return static_cast<std::int64_t>((u.QuadPart / 10000000ull) - kEpochDiff);
}
