#pragma once

#include <cstddef>

/* Timetable for EventsPad — UTC clock math only (no game memory / live API state). */
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
		bool        inDefaultAll; /* false → invasions/festivals/fractals stay filtered */
		int         activeSec;
		int         cycleSec;   /* unused for DayList */
		int         phaseSec;   /* Repeat / CycleSlot offset */
		int         copies;     /* evenly spaced copies in cycle (usually 1) */
		const int*  starts;     /* DayList / CycleList */
		int         startCount;
	};

	const Entry* All(size_t* outCount);
	const char* const* Sections(size_t* outCount);
	int IndexOfKey(const char* key);

	/* True if this timetable row belongs on the given Mumble / API map id. */
	bool EntryMatchesMap(const Entry& e, unsigned mapId);
	/* Short label for UI (nullptr if unknown). */
	const char* MapDisplayName(unsigned mapId);
}
