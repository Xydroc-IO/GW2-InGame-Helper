#pragma once

#include <string>

/* Addon-owned panel hotkeys (GetAsyncKeyState poll - not Nexus InputBinds).
   Rebind in Settings -> Keybinds. Helper open (Ctrl+Shift+H) stays Nexus for QuickAccess. */
namespace PanelBinds
{
	enum Slot : int
	{
		Account = 0,
		Pathing,
		Events,
		Notes,
		Completion,
		Farming,
		Economy,
		Instances,
		Logs,
		Compass,
		SettingsPad,
		Wallet,
		Vault,
		TpWatch,
		Lookup,
		Marker,
		Watch,
		Achievements,
		Count
	};

	struct Chord
	{
		bool     ctrl = false;
		bool     shift = false;
		bool     alt = false;
		unsigned vk = 0; /* 0 = unbound */
	};

	struct State
	{
		Chord chords[Count]{};
		int   captureTarget = -1; /* Slot listening; -1 = none */
	};

	State& Get();

	void SetDefaults();
	void Poll();
	void DrawSettingsTab();

	std::string FormatChord(const Chord& c);
	bool ParseChord(const char* s, Chord& out);
	std::string Serialize();
	void Deserialize(const char* s);

	const char* SlotLabel(Slot s);
	const char* VkDisplayName(unsigned vk);

	/* One-shot: drop stale Nexus panel bind ids after upgrade. */
	void DeregisterLegacyNexusBinds();
}
