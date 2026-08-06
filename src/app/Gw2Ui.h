#pragma once

/* GW2.dat UI textures via assets.gw2dat.com (same pipeline as Blish DatAssetCache).
   Curated IDs verified on the CDN — not wiki scrapes. */

namespace Gw2Ui
{
	/* Well-known UI asset IDs (stable). Prefer gold/idle variants for rails. */
	enum class Icon : int
	{
		Close        = 155014, /* navy X */
		Check        = 155023, /* green check */
		CheckPlain   = 154980, /* white tick */
		Cancel       = 154983, /* slash-circle */
		Alert        = 155018, /* red ! */
		GoldCoins    = 155025,
		Gem          = 155024,
		Achievements = 155008, /* A badge */
		Back         = 155009, /* red B */
		Story        = 155015, /* journal Y */
		LockBag      = 156669,
		Bag          = 156670,
		/* Main-menu chrome (gold idle). */
		Hero         = 157085, /* sword + shield */
		Guild        = 157088, /* lion */
		Trade        = 157090, /* Black Lion */
		Inventory    = 157098, /* pouch */
		Mail         = 157106,
		Options      = 157109, /* gear */
		Contacts     = 157112,
		Squad        = 157116, /* crown */
		PvP          = 157119, /* crossed swords */
		Map          = 157122, /* compass rose */
		Help         = 157095, /* ? */
		Logout       = 157092,
		DetailsCrest = 605004,
		Sample       = 102491,
	};

	/* Request Nexus upload from assets.gw2dat.com/<id>.png */
	void Request(int assetId);

	inline void Request(Icon icon) { Request(static_cast<int>(icon)); }

	/* Warm a small set used by pads/rails. Safe every frame. */
	void WarmCommon();

	/* Draw texture if ready. */
	bool Image(int assetId, float size = 24.f);
	bool Image(Icon icon, float size = 24.f);

	/* ImageButton; falls back to text label if texture not ready. */
	bool IconButton(const char* id, int assetId, float size = 26.f);
	bool IconButton(const char* id, Icon icon, float size = 26.f);

	/* Icon + label button (icon left of text). */
	bool IconLabelButton(const char* label, int assetId, float iconSize = 20.f);
	bool IconLabelButton(const char* label, Icon icon, float iconSize = 20.f);

	/* Full-width rail toggle with optional DAT icon (PadNav / helper chrome). */
	bool RailToggle(const char* label, bool on, int assetId = 0, float iconSize = 18.f);
	bool RailToggle(const char* label, bool on, Icon icon, float iconSize = 18.f);
}
