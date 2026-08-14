#include "EventsData.h"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace EventsData
{
	namespace
	{
		int PosMod(int v, int m)
		{
			if (m <= 0) return 0;
			int r = v % m;
			if (r < 0) r += m;
			return r;
		}

		Timing FromStartList(int nowInCycle, int cycleLen, const int* starts, int n, int activeSec)
		{
			Timing t;
			if (!starts || n <= 0 || activeSec <= 0)
				return t;
			for (int i = 0; i < n; ++i)
			{
				const int begin = starts[i];
				if (nowInCycle < begin)
				{
					t.untilStart = begin - nowInCycle;
					return t;
				}
				if (nowInCycle < begin + activeSec)
				{
					t.live = true;
					t.untilStart = 0;
					t.untilEnd = begin + activeSec - nowInCycle;
					return t;
				}
			}
			t.untilStart = cycleLen - nowInCycle + starts[0];
			return t;
		}

		Timing FromRepeat(time_t now, int cycleSec, int phaseSec, int activeSec, int copies)
		{
			Timing t;
			if (cycleSec <= 0 || activeSec <= 0)
				return t;
			if (copies < 1) copies = 1;
			const int nowIn = static_cast<int>(now % cycleSec);
			const int stride = cycleSec / copies;
			int bestWait = cycleSec;
			for (int c = 0; c < copies; ++c)
			{
				const int start = phaseSec + c * stride;
				const int age = PosMod(nowIn - start, cycleSec);
				if (age < activeSec)
				{
					t.live = true;
					t.untilStart = 0;
					t.untilEnd = activeSec - age;
					return t;
				}
				const int wait = cycleSec - age;
				if (wait < bestWait)
					bestWait = wait;
			}
			t.untilStart = bestWait;
			return t;
		}
	}

	Timing ComputeTiming(const Entry& e, time_t now)
	{
		switch (e.sched)
		{
		case Sched::DayList:
			return FromStartList(static_cast<int>(now % 86400), 86400,
				e.starts, e.startCount, e.activeSec);
		case Sched::CycleList:
			return FromStartList(static_cast<int>(now % e.cycleSec), e.cycleSec,
				e.starts, e.startCount, e.activeSec);
		case Sched::Repeat:
		case Sched::CycleSlot:
		default:
			return FromRepeat(now, e.cycleSec, e.phaseSec, e.activeSec, e.copies);
		}
	}

	void FormatUtcClock(time_t t, char* out, size_t outLen)
	{
		if (!out || outLen == 0)
			return;
		out[0] = '\0';
		std::tm tm{};
#if defined(_WIN32)
		gmtime_s(&tm, &t);
#else
		gmtime_r(&t, &tm);
#endif
		std::snprintf(out, outLen, "%02d:%02d UTC", tm.tm_hour, tm.tm_min);
	}

	void FormatNextUtcHint(const Entry& e, time_t now, char* out, size_t outLen)
	{
		if (!out || outLen == 0)
			return;
		out[0] = '\0';
		const Timing t = ComputeTiming(e, now);
		if (t.live)
		{
			char endClk[32]{};
			FormatUtcClock(now + (t.untilEnd > 0 ? t.untilEnd : 0), endClk, sizeof(endClk));
			if (IsSpawnLive(e, t))
				std::snprintf(out, outLen, "LIVE until %s", endClk);
			else
				std::snprintf(out, outLen, "Phase until %s", endClk);
			return;
		}
		if (t.untilStart < 0)
			return;
		char nextClk[32]{};
		FormatUtcClock(now + t.untilStart, nextClk, sizeof(nextClk));
		/* Second slot: peek ahead after this window. */
		const time_t after = now + t.untilStart + (e.activeSec > 0 ? e.activeSec : 1);
		const Timing t2 = ComputeTiming(e, after);
		char next2[32]{};
		if (t2.untilStart >= 0)
			FormatUtcClock(after + t2.untilStart, next2, sizeof(next2));
		if (next2[0])
			std::snprintf(out, outLen, "Next %s · then %s", nextClk, next2);
		else
			std::snprintf(out, outLen, "Next %s", nextClk);
	}

	const Entry* FindByKey(const char* key)
	{
		if (!key || !key[0])
			return nullptr;
		size_t n = 0;
		const Entry* all = All(&n);
		for (size_t i = 0; i < n; ++i)
		{
			if (all[i].key && std::strcmp(all[i].key, key) == 0)
				return &all[i];
		}
		return nullptr;
	}
}
