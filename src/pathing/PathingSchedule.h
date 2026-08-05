#pragma once

#include <cstdint>
#include <string>

/* Blish schedule / schedule-duration (UTC cron windows). */
namespace PathingSchedule
{
	bool IsActiveUtc(const std::string& cronFiveField, float durationMinutes,
		std::int64_t nowUnixUtc);

	bool MarkerActive(const char* schedule, float scheduleDurationMin,
		std::int64_t nowUnixUtc);

	/* Current UTC unix seconds (Win32 FILETIME). */
	std::int64_t NowUnixUtc();
}
