#include "CompletionApIds.h"

#include <cstring>

namespace CompletionDetail
{
	namespace
	{
		/* Official /v2/achievements ids (Explorer + Been There). */
		const ApIdRow kRows[] = {
			{ 0, 137, "Been There, Done That", "" },
			/* Region explorers (Core) — used when no zone-specific row */
			{ 15, 12, "Krytan Explorer", "" },
			{ 17, 12, "Krytan Explorer", "" },
			{ 18, 12, "Krytan Explorer", "" },
			{ 23, 12, "Krytan Explorer", "" },
			{ 24, 12, "Krytan Explorer", "" },
			{ 50, 12, "Krytan Explorer", "" },
			{ 73, 12, "Krytan Explorer", "" },
			{ 873, 12, "Krytan Explorer", "" },
			{ 19, 101, "Ascalon Explorer", "" },
			{ 20, 101, "Ascalon Explorer", "" },
			{ 21, 101, "Ascalon Explorer", "" },
			{ 22, 101, "Ascalon Explorer", "" },
			{ 25, 101, "Ascalon Explorer", "" },
			{ 32, 101, "Ascalon Explorer", "" },
			{ 218, 101, "Ascalon Explorer", "" },
			{ 26, 100, "Shiverpeak Explorer", "" },
			{ 27, 100, "Shiverpeak Explorer", "" },
			{ 28, 100, "Shiverpeak Explorer", "" },
			{ 29, 100, "Shiverpeak Explorer", "" },
			{ 30, 100, "Shiverpeak Explorer", "" },
			{ 31, 100, "Shiverpeak Explorer", "" },
			{ 326, 100, "Shiverpeak Explorer", "" },
			{ 34, 102, "Maguuma Explorer", "" },
			{ 35, 102, "Maguuma Explorer", "" },
			{ 39, 102, "Maguuma Explorer", "" },
			{ 53, 102, "Maguuma Explorer", "" },
			{ 54, 102, "Maguuma Explorer", "" },
			{ 91, 102, "Maguuma Explorer", "" },
			{ 139, 102, "Maguuma Explorer", "" },
			{ 51, 103, "Orr Explorer", "" },
			{ 62, 103, "Orr Explorer", "" },
			{ 65, 103, "Orr Explorer", "" },
			/* Zone explorers */
			{ 1042, 2222, "Verdant Brink Explorer", "" },
			{ 1052, 2222, "Verdant Brink Explorer", "" },
			{ 1043, 2370, "Auric Basin Explorer", "" },
			{ 1045, 2378, "Tangled Depths Explorer", "" },
			{ 1041, 2212, "Dragon's Stand Explorer", "" },
			{ 1210, 3864, "Crystal Oasis Explorer", "" },
			{ 1211, 3616, "Desert Highlands Explorer", "" },
			{ 1228, 3623, "Elon Riverlands Explorer", "" },
			{ 1226, 3612, "Desolation Explorer", "" },
			{ 1248, 3865, "Domain of Vabbi Explorer", "" },
			{ 1442, 6077, "Seitung Province Explorer", "" },
			{ 1438, 6142, "New Kaineng City Explorer", "" },
			{ 1452, 6481, "Echovald Wilds Explorer", "" },
			{ 1422, 6248, "Dragon's End Explorer", "" },
			{ 1510, 7147, "Skywatch Archipelago Explorer", "" },
			{ 1517, 7054, "Amnytas Explorer", "" },
			{ 1526, 8101, "Inner Nayos Explorer", "" },
			{ 1550, 8154, "Lowland Shore Explorer", "" },
			{ 1554, 8283, "Janthir Syntri Explorer", "" },
			{ 1575, 8625, "Mistburned Barrens Explorer", "" },
			{ 1574, 8704, "Bava Nisos Explorer", "" },
		};
	}

	const ApIdRow* ApIdTable(size_t* outCount)
	{
		if (outCount)
			*outCount = sizeof(kRows) / sizeof(kRows[0]);
		return kRows;
	}

	const ApIdRow* ApIdForMap(uint32_t mapId)
	{
		if (mapId == 0)
			return nullptr;
		const ApIdRow* best = nullptr;
		for (const ApIdRow& r : kRows)
		{
			if (r.mapId == mapId)
				return &r;
		}
		(void)best;
		return nullptr;
	}

	const ApIdRow* ApIdForPackPrefix(const char* packType)
	{
		if (!packType || !packType[0])
			return nullptr;
		size_t n = 0;
		const ApIdRow* rows = ApIdTable(&n);
		const size_t plen = std::strlen(packType);
		const ApIdRow* best = nullptr;
		size_t bestLen = 0;
		for (size_t i = 0; i < n; ++i)
		{
			const char* p = rows[i].packPrefix;
			if (!p || !p[0])
				continue;
			const size_t l = std::strlen(p);
			if (l > plen)
				continue;
			if (std::strncmp(packType, p, l) != 0)
				continue;
			if (l < plen && packType[l] != '.')
				continue;
			if (l > bestLen)
			{
				bestLen = l;
				best = &rows[i];
			}
		}
		return best;
	}
}
