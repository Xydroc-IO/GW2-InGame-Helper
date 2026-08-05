#pragma once

#include "TrailToolsShared.h"

#include <cstdint>
#include <string>

/* Addon-owned Trail Tools keybinds (GetAsyncKeyState poll — not Nexus InputBinds).
   Place-marker slots exceed TacO’s 4–5 so mount / route markers can each have a chord. */
namespace TrailToolsBinds
{
	constexpr int kPlaceSlots = 10;

	struct Chord
	{
		bool     ctrl = false;
		bool     shift = false;
		bool     alt = false;
		unsigned vk = 0; /* 0 = unbound */
	};

	struct PlaceSlot
	{
		Chord chord;
		char  type[160] = {};  /* MarkerCategory path; empty → gDraft.markerType */
		char  label[48] = {};  /* optional UI name e.g. "Skyscale" */
	};

	struct State
	{
		Chord     trailStart{};
		Chord     trailPause{};
		Chord     trailSection{};
		Chord     trailDeleteSeg{};
		Chord     markerDelete{};
		PlaceSlot place[kPlaceSlots]{};
		bool      trailRecording = false;
		bool      trailPaused = false;
		int       captureTarget = -1; /* UI: which row is listening; -1 = none */
	};

	State& Get();

	void SetDefaults();
	void Poll(); /* edge-trigger chords + auto-sample while recording */

	/* Actions (also used by Trails / Markers tabs). */
	void ActionTrailStart();
	void ActionTrailPause();
	void ActionTrailSection();
	void ActionTrailDeleteSeg();
	void ActionPlaceMarker(int slotIndex); /* -1 = default markerType */
	void ActionDeleteMarker();

	std::string FormatChord(const Chord& c);
	bool ParseChord(const char* s, Chord& out);
	std::string Serialize();
	void Deserialize(const char* s);

	const char* VkDisplayName(unsigned vk);
}
