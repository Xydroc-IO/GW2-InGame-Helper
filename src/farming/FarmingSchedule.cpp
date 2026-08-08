#include "FarmingShared.h"

#include "EventsData.h"

#include <cstdio>
#include <ctime>

namespace FarmingDetail
{
	bool RunScheduleHint(const Run& r, char* out, size_t outLen)
	{
		if (!out || outLen == 0)
			return false;
		out[0] = '\0';
		if (!r.eventsKey[0])
			return false;
		const EventsData::Entry* e = EventsData::FindByKey(r.eventsKey);
		if (!e)
			return false;
		EventsData::FormatNextUtcHint(*e, std::time(nullptr), out, outLen);
		return out[0] != '\0';
	}
}
