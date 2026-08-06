#pragma once

#include "imgui/imgui.h"

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
		/* Blish StandardWindow chrome (UiChrome file pack). */
		PanelFill      = 155985, /* docs StandardWindow background */
		PanelFillAlt   = 155981,
		WindowEmblem   = 156022,
		WindowCorner   = 156008,
		WindowCornerBr = 156009,
		WindowResize   = 156010,
	};

	/* Request Nexus upload from assets.gw2dat.com/<id>.png */
	void Request(int assetId);

	inline void Request(Icon icon) { Request(static_cast<int>(icon)); }

	/* Warm a small set used by pads/rails. Safe every frame. */
	void WarmCommon();

	/* Draw texture if ready. */
	bool Image(int assetId, float size = 24.f);
	bool Image(Icon icon, float size = 24.f);

	/* Paint Immersive pad chrome from extracted ui-chrome pack. Call after Begin.
	   Returns true when panel fill was drawn. */
	bool PaintPadChrome(float opacity = 1.f);

	/* Blish-style title row: emblem + gold title + minimize + close.
	   Call after PaintPadChrome when using ImGuiWindowFlags_NoTitleBar.
	   Returns false when the pad is minimized (skip body widgets). */
	bool DrawPadTitleBar(const char* title, bool* pOpen, float opacity = 1.f);

	/* Flags for pads that use PaintPadChrome + DrawPadTitleBar. */
	inline ImGuiWindowFlags PadWindowFlags(ImGuiWindowFlags extra = 0)
	{
		return ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | extra;
	}

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
