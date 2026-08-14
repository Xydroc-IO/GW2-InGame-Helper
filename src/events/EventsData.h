#pragma once

#include <cstddef>
#include <ctime>

/* Timetable for EventsPad - UTC clock math only (no game memory / live API state). */
namespace EventsData
{
	enum class Sched : unsigned char
	{
		Repeat = 0,     /* repeats every `cycleSec`, first phase at `phaseSec` */
		DayList = 1,    /* fixed start times each UTC day (`starts` / `startCount`) */
		CycleSlot = 2,  /* one (or `copies`) phase(s) inside a map meta `cycleSec` */
		CycleList = 3,  /* irregular starts inside each meta `cycleSec` */
	};

	struct Entry
	{
		const char* key;
		const char* section;
		const char* mapLabel;   /* empty for world bosses / one-offs */
		const char* title;
		const char* waypoint;
		const char* bossApi;    /* /v2/account/worldbosses id, or empty */
		const char* chestApi;   /* /v2/account/mapchests id, or empty */
		Sched       sched;
		bool        inDefaultAll; /* false -> invasions/festivals/fractals stay filtered */
		int         activeSec;
		int         cycleSec;   /* unused for DayList */
		int         phaseSec;   /* Repeat / CycleSlot offset */
		int         copies;     /* evenly spaced copies in cycle (usually 1) */
		const int*  starts;     /* DayList / CycleList */
		int         startCount;
	};

	struct Timing
	{
		bool live = false;
		int  untilStart = -1; /* 0 when live */
		int  untilEnd = -1;
	};

	const Entry* All(size_t* outCount);
	const char* const* Sections(size_t* outCount);
	int IndexOfKey(const char* key);
	const Entry* FindByKey(const char* key);

	Timing ComputeTiming(const Entry& e, time_t now);
	/* LIVE chip / spawn-live toast: window open and ≤45 minutes.
	   Hour-plus HoT day/outpost phases (75–90m) stay off LIVE. */
	constexpr int kSpawnLiveMaxSec = 45 * 60;
	inline bool IsSpawnLive(const Entry& e, const Timing& t)
	{
		return t.live && e.activeSec > 0 && e.activeSec <= kSpawnLiveMaxSec;
	}
	void FormatUtcClock(time_t t, char* out, size_t outLen);
	/* One-line "Next HH:MM UTC · then HH:MM UTC" (or LIVE until ...). */
	void FormatNextUtcHint(const Entry& e, time_t now, char* out, size_t outLen);

	/* True if this timetable row belongs on the given Mumble / API map id. */
	bool EntryMatchesMap(const Entry& e, unsigned mapId);
	/* Short label for UI (nullptr if unknown). */
	const char* MapDisplayName(unsigned mapId);
}
