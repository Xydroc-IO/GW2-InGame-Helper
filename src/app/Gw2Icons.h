#pragma once

#include <cstddef>

/* Official GW2 render-service icons (render.guildwars2.com) — Blish-style,
   not wiki scrapes. Resolve by item id or full icon URL; Nexus uploads texture. */

namespace Gw2Icons
{
	/* Remember icon URL from a /v2/items (or skins) JSON object fragment. */
	void RememberIcon(int id, const char* renderUrl);

	/* Parse "icon":"https://render..." from a JSON object starting at brace. */
	void RememberIconFromJson(int id, const char* json, size_t brace, size_t end);

	/* Queue item-id resolve via /v2/items if URL unknown. Safe on UI thread. */
	void RequestItem(int itemId);

	/* Kick Nexus Textures_GetOrCreateFromURL when we already have a render URL. */
	void RequestUrl(const char* renderUrl);

	/* Background batch resolver — call once per frame from UI (cheap). */
	void Tick();

	/* Draw if icon already remembered/resolved (no item-API queue). */
	bool Image(int id, float size = 28.f);

	/* RequestItem + Image — for commerce / stash item rows. */
	bool ImageItem(int itemId, float size = 28.f);

	/* Wallet currencies — separate from item ids (currency 1 ≠ item 1). */
	void RememberCurrencyIcon(int currencyId, const char* renderUrl);
	void RememberCurrencyIconFromJson(int currencyId, const char* json, size_t brace, size_t end);
	bool ImageCurrency(int currencyId, float size = 28.f);
	bool HasCurrencyIcon(int currencyId);

	/* Profession icons (/v2/professions id, e.g. "Guardian"). */
	void RememberProfessionIcon(const char* professionId, const char* renderUrl);
	bool ImageProfession(const char* professionId, float size = 28.f);
	/* Warm built-in render URLs so roster icons work offline of a fetch. */
	void WarmProfessionIcons();

	/* Same for an arbitrary render URL (Lookup detail). */
	bool ImageUrl(const char* renderUrl, float size = 36.f);
}
