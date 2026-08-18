#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/* Account unlock sets from official /v2/account APIs + public name catalogs.
   One module, many kinds - AccountPad / UnlocksPad only render. */
namespace UnlocksData
{
	enum class Kind
	{
		Skins = 0,
		Dyes,
		Minis,
		Finishers,
		Outfits,
		Gliders,
		MailCarriers,
		Novelties,
		Titles,
		Count
	};

	struct Row
	{
		int id = 0;
		std::string name;
		unsigned rgb = 0; /* 0xRRGGBB — dyes only */
		bool hasRgb = false;
		std::string iconUrl; /* render.guildwars2.com — never an item-id lookup */
	};

	const char* KindLabel(Kind k);
	const char* KindApiPath(Kind k); /* account path fragment */

	void EnsureLoaded(Kind k, bool force = false);
	void EnsureAll(bool force = false);
	void Tick();
	bool Busy(Kind k);
	bool BusyAny();
	bool Ready(Kind k);
	const char* Status(Kind k);

	size_t Count(Kind k);
	bool Has(Kind k, int id);
	void Search(Kind k, const char* query, std::vector<Row>& out, size_t maxN = 10000);
}
