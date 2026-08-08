#pragma once

#include <cstdint>
#include <cstddef>

/* Curated official achievement ids for Completion API overlay.
   Map % is NOT synced — Explorer / Been There only. */
namespace CompletionDetail
{
	struct ApIdRow
	{
		uint32_t mapId;      /* 0 = account-wide */
		uint32_t achievementId;
		const char* label;
		const char* packPrefix; /* optional Lady AP type prefix; may be empty */
	};

	const ApIdRow* ApIdTable(size_t* outCount);
	/* Best Explorer (or pack) row for a map; nullptr if none. */
	const ApIdRow* ApIdForMap(uint32_t mapId);
	const ApIdRow* ApIdForPackPrefix(const char* packType);
}
