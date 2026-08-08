#include "PanelBinds.h"

#include "AccountPad.h"
#include "CompletionPad.h"
#include "DirectionCompass.h"
#include "EconomyPad.h"
#include "EventsPad.h"
#include "FarmingPad.h"
#include "Globals.h"
#include "InstancesPad.h"
#include "LogManagerPad.h"
#include "LookupPad.h"
#include "NotesPad.h"
#include "PathingGuidesPad.h"
#include "PathingTrails.h"
#include "Settings.h"
#include "SettingsPad.h"
#include "TpWatchPad.h"
#include "TrailToolsPad.h"
#include "VaultPad.h"
#include "WatchCapture.h"
#include "WatchPad.h"
#include "WalletPad.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

namespace
{
	using PanelBinds::Chord;
	using PanelBinds::Slot;
	using PanelBinds::State;
	using PanelBinds::Count;

	State gState{};
	bool  gHeld[Count]{};
	bool  gDefaultsApplied = false;
	DWORD gLastFireMs = 0;

	bool KeyDown(int vk)
	{
		return (GetAsyncKeyState(vk) & 0x8000) != 0;
	}

	bool ModsMatch(const Chord& c)
	{
		const bool ctrl = KeyDown(VK_CONTROL) || KeyDown(VK_LCONTROL) || KeyDown(VK_RCONTROL);
		const bool shift = KeyDown(VK_SHIFT) || KeyDown(VK_LSHIFT) || KeyDown(VK_RSHIFT);
		const bool alt = KeyDown(VK_MENU) || KeyDown(VK_LMENU) || KeyDown(VK_RMENU);
		return ctrl == c.ctrl && shift == c.shift && alt == c.alt;
	}

	bool ChordDown(const Chord& c)
	{
		return c.vk != 0 && ModsMatch(c) && KeyDown(static_cast<int>(c.vk));
	}

	bool Edge(int idx, bool down)
	{
		const bool was = gHeld[idx];
		gHeld[idx] = down;
		return down && !was;
	}

	bool TypingBlocked()
	{
		const ImGuiIO& io = ImGui::GetIO();
		return io.WantTextInput;
	}

	bool FireDebounce()
	{
		const DWORD now = GetTickCount();
		if (now - gLastFireMs < 250)
			return false;
		gLastFireMs = now;
		return true;
	}

	void CloseOrOpen(bool& show, void (*open)())
	{
		if (show)
		{
			show = false;
			Settings::SetDirty();
		}
		else
			open();
	}

	void FireSlot(Slot s)
	{
		if (!FireDebounce())
			return;
		switch (s)
		{
		case Slot::Account: CloseOrOpen(G::ShowAccount, &AccountPad::OpenAndRefresh); break;
		case Slot::Pathing: CloseOrOpen(G::ShowPathingGuides, &PathingGuidesPad::Open); break;
		case Slot::Events: CloseOrOpen(G::ShowEvents, &EventsPad::OpenAndRefresh); break;
		case Slot::Notes: CloseOrOpen(G::ShowNotes, &NotesPad::Open); break;
		case Slot::Completion: CloseOrOpen(G::ShowCompletion, &CompletionPad::OpenAndRefresh); break;
		case Slot::Farming: CloseOrOpen(G::ShowFarming, &FarmingPad::OpenAndRefresh); break;
		case Slot::Economy: CloseOrOpen(G::ShowEconomy, &EconomyPad::OpenAndRefresh); break;
		case Slot::Instances: CloseOrOpen(G::ShowInstances, &InstancesPad::OpenAndRefresh); break;
		case Slot::Logs: CloseOrOpen(G::ShowLogManager, &LogManagerPad::OpenAndRefresh); break;
		case Slot::TrailTools: CloseOrOpen(G::ShowTrailTools, &TrailToolsPad::Open); break;
		case Slot::Compass: CloseOrOpen(G::ShowCompassPad, &DirectionCompass::Open); break;
		case Slot::Watch:
			WatchPad::ToggleControl();
			break;
		case Slot::SettingsPad: CloseOrOpen(G::ShowSettings, &SettingsPad::Open); break;
		case Slot::Wallet: CloseOrOpen(G::ShowWallet, &WalletPad::OpenAndRefresh); break;
		case Slot::Vault: CloseOrOpen(G::ShowVault, &VaultPad::OpenAndRefresh); break;
		case Slot::TpWatch: CloseOrOpen(G::ShowTpWatch, &TpWatchPad::OpenAndRefresh); break;
		case Slot::Lookup: CloseOrOpen(G::ShowLookup, &LookupPad::OpenAndLookup); break;
		case Slot::Marker: PathingTrails::RequestMarkerInteract(); break;
		default: break;
		}
	}

	struct VkName
	{
		unsigned    vk;
		const char* name;
	};

	const VkName kVkNames[] = {
		{ VK_NUMPAD0, "NUMPAD0" }, { VK_NUMPAD1, "NUMPAD1" }, { VK_NUMPAD2, "NUMPAD2" },
		{ VK_NUMPAD3, "NUMPAD3" }, { VK_NUMPAD4, "NUMPAD4" }, { VK_NUMPAD5, "NUMPAD5" },
		{ VK_NUMPAD6, "NUMPAD6" }, { VK_NUMPAD7, "NUMPAD7" }, { VK_NUMPAD8, "NUMPAD8" },
		{ VK_NUMPAD9, "NUMPAD9" }, { VK_MULTIPLY, "NUMPAD*" }, { VK_ADD, "NUMPAD+" },
		{ VK_SUBTRACT, "NUMPAD-" }, { VK_DIVIDE, "NUMPAD/" }, { VK_DECIMAL, "NUMPAD." },
		{ VK_BACK, "BACKSPACE" }, { VK_DELETE, "DELETE" }, { VK_INSERT, "INSERT" },
		{ VK_HOME, "HOME" }, { VK_END, "END" }, { VK_PRIOR, "PAGEUP" }, { VK_NEXT, "PAGEDOWN" },
		{ VK_SPACE, "SPACE" }, { VK_OEM_COMMA, "," }, { VK_OEM_PERIOD, "." },
		{ VK_OEM_MINUS, "-" }, { VK_OEM_PLUS, "=" }, { VK_OEM_1, ";" }, { VK_OEM_2, "/" },
		{ VK_F1, "F1" }, { VK_F2, "F2" }, { VK_F3, "F3" }, { VK_F4, "F4" },
		{ VK_F5, "F5" }, { VK_F6, "F6" }, { VK_F7, "F7" }, { VK_F8, "F8" },
		{ VK_F9, "F9" }, { VK_F10, "F10" }, { VK_F11, "F11" }, { VK_F12, "F12" },
	};

	unsigned VkFromName(const char* name)
	{
		if (!name || !*name)
			return 0;
		if (std::strlen(name) == 1)
		{
			const char c = name[0];
			if (c >= 'A' && c <= 'Z')
				return static_cast<unsigned>(c);
			if (c >= 'a' && c <= 'z')
				return static_cast<unsigned>(c - 'a' + 'A');
			if (c >= '0' && c <= '9')
				return static_cast<unsigned>(c);
		}
		for (const auto& e : kVkNames)
		{
			if (_stricmp(e.name, name) == 0)
				return e.vk;
		}
		return 0;
	}

	const char* kSlotKeys[Count] = {
		"account", "pathing", "events", "notes", "completion", "farming",
		"economy", "instances", "logs", "trailtools", "compass", "settings",
		"wallet", "vault", "tpwatch", "lookup", "marker", "watch"
	};
	static_assert(sizeof(kSlotKeys) / sizeof(kSlotKeys[0]) == static_cast<size_t>(Count),
		"kSlotKeys must match PanelBinds::Count");

	void EnsureDefaults()
	{
		if (gDefaultsApplied)
			return;
		PanelBinds::SetDefaults();
		gDefaultsApplied = true;
	}
}

PanelBinds::State& PanelBinds::Get()
{
	EnsureDefaults();
	return gState;
}

const char* PanelBinds::SlotLabel(Slot s)
{
	switch (s)
	{
	case Slot::Account: return "Account";
	case Slot::Pathing: return "Pathing";
	case Slot::Events: return "Events";
	case Slot::Notes: return "Notes";
	case Slot::Completion: return "Completion";
	case Slot::Farming: return "Farming";
	case Slot::Economy: return "Economy";
	case Slot::Instances: return "Instances";
	case Slot::Logs: return "DPS Logs";
	case Slot::TrailTools: return "Trail Tools";
	case Slot::Compass: return "Compass";
	case Slot::Watch: return "Watch";
	case Slot::SettingsPad: return "Settings";
	case Slot::Wallet: return "Wallet";
	case Slot::Vault: return "Vault";
	case Slot::TpWatch: return "TP Watch";
	case Slot::Lookup: return "Lookup";
	case Slot::Marker: return "Marker interact";
	default: return "?";
	}
}

const char* PanelBinds::VkDisplayName(unsigned vk)
{
	if (vk == 0)
		return "";
	for (const auto& e : kVkNames)
	{
		if (e.vk == vk)
			return e.name;
	}
	static char buf[8];
	if (vk >= 'A' && vk <= 'Z')
	{
		buf[0] = static_cast<char>(vk);
		buf[1] = 0;
		return buf;
	}
	if (vk >= '0' && vk <= '9')
	{
		buf[0] = static_cast<char>(vk);
		buf[1] = 0;
		return buf;
	}
	std::snprintf(buf, sizeof(buf), "0x%02X", vk);
	return buf;
}

std::string PanelBinds::FormatChord(const Chord& c)
{
	if (c.vk == 0)
		return "Unbound";
	std::string s;
	if (c.ctrl) s += "CTRL+";
	if (c.shift) s += "SHIFT+";
	if (c.alt) s += "ALT+";
	s += VkDisplayName(c.vk);
	return s;
}

bool PanelBinds::ParseChord(const char* s, Chord& out)
{
	out = {};
	if (!s || !*s || _stricmp(s, "Unbound") == 0 || _stricmp(s, "none") == 0)
		return true;
	char buf[96]{};
	std::snprintf(buf, sizeof(buf), "%s", s);
	for (char* p = buf; *p; ++p)
		if (*p >= 'a' && *p <= 'z')
			*p = static_cast<char>(*p - 'a' + 'A');
	char* tok = buf;
	while (tok && *tok)
	{
		char* plus = std::strchr(tok, '+');
		if (plus)
			*plus = 0;
		if (std::strcmp(tok, "CTRL") == 0 || std::strcmp(tok, "CONTROL") == 0)
			out.ctrl = true;
		else if (std::strcmp(tok, "SHIFT") == 0)
			out.shift = true;
		else if (std::strcmp(tok, "ALT") == 0 || std::strcmp(tok, "MENU") == 0)
			out.alt = true;
		else
			out.vk = VkFromName(tok);
		tok = plus ? plus + 1 : nullptr;
	}
	return out.vk != 0;
}

void PanelBinds::SetDefaults()
{
	gState = {};
	ParseChord("CTRL+SHIFT+A", gState.chords[Account]);
	ParseChord("CTRL+SHIFT+G", gState.chords[Pathing]);
	ParseChord("CTRL+SHIFT+E", gState.chords[Events]);
	ParseChord("CTRL+SHIFT+N", gState.chords[Notes]);
	ParseChord("CTRL+SHIFT+M", gState.chords[Completion]);
	ParseChord("CTRL+SHIFT+R", gState.chords[Farming]);
	ParseChord("CTRL+SHIFT+Y", gState.chords[Economy]);
	ParseChord("CTRL+SHIFT+I", gState.chords[Instances]);
	ParseChord("CTRL+SHIFT+L", gState.chords[Logs]);
	ParseChord("CTRL+SHIFT+B", gState.chords[TrailTools]);
	ParseChord("CTRL+SHIFT+O", gState.chords[Compass]);
	ParseChord("CTRL+SHIFT+W", gState.chords[Watch]);
	ParseChord("CTRL+SHIFT+.", gState.chords[SettingsPad]);
	ParseChord("CTRL+SHIFT+U", gState.chords[Wallet]);
	ParseChord("CTRL+SHIFT+V", gState.chords[Vault]);
	ParseChord("CTRL+SHIFT+P", gState.chords[TpWatch]);
	ParseChord("CTRL+SHIFT+J", gState.chords[Lookup]);
	ParseChord("CTRL+SHIFT+F", gState.chords[Marker]);
	gDefaultsApplied = true;
}

void PanelBinds::Poll()
{
	EnsureDefaults();

	if (gState.captureTarget >= 0)
	{
		const bool ctrl = KeyDown(VK_CONTROL) || KeyDown(VK_LCONTROL) || KeyDown(VK_RCONTROL);
		const bool shift = KeyDown(VK_SHIFT) || KeyDown(VK_LSHIFT) || KeyDown(VK_RSHIFT);
		const bool alt = KeyDown(VK_MENU) || KeyDown(VK_LMENU) || KeyDown(VK_RMENU);
		for (int vk = 1; vk < 256; ++vk)
		{
			if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
				vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
				vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
				vk == VK_LWIN || vk == VK_RWIN || vk == VK_CAPITAL || vk == VK_NUMLOCK ||
				vk == VK_SCROLL || vk == VK_ESCAPE)
				continue;
			if (!KeyDown(vk))
				continue;
			const int t = gState.captureTarget;
			if (t >= 0 && t < Count)
			{
				Chord c;
				c.ctrl = ctrl;
				c.shift = shift;
				c.alt = alt;
				c.vk = static_cast<unsigned>(vk);
				gState.chords[t] = c;
			}
			gState.captureTarget = -1;
			Settings::SetDirty();
			std::memset(gHeld, 0, sizeof(gHeld));
			return;
		}
		if (KeyDown(VK_ESCAPE))
			gState.captureTarget = -1;
		return;
	}

	if (TypingBlocked())
		return;

	for (int i = 0; i < Count; ++i)
	{
		if (Edge(i, ChordDown(gState.chords[i])))
			FireSlot(static_cast<Slot>(i));
	}
}

std::string PanelBinds::Serialize()
{
	EnsureDefaults();
	std::string o;
	for (int i = 0; i < Count; ++i)
	{
		o += kSlotKeys[i];
		o += '=';
		o += FormatChord(gState.chords[i]);
		o += ';';
	}
	return o;
}

void PanelBinds::Deserialize(const char* s)
{
	SetDefaults();
	if (!s || !*s)
		return;
	char buf[2048]{};
	std::snprintf(buf, sizeof(buf), "%s", s);
	char* tok = buf;
	while (tok && *tok)
	{
		char* semi = std::strchr(tok, ';');
		if (semi)
			*semi = 0;
		char* eq = std::strchr(tok, '=');
		if (eq)
		{
			*eq = 0;
			const char* key = tok;
			const char* val = eq + 1;
			for (int i = 0; i < Count; ++i)
			{
				if (_stricmp(key, kSlotKeys[i]) == 0)
				{
					ParseChord(val, gState.chords[i]);
					break;
				}
			}
		}
		tok = semi ? semi + 1 : nullptr;
	}
	gDefaultsApplied = true;
}

void PanelBinds::DeregisterLegacyNexusBinds()
{
	if (!G::API || !G::API->InputBinds_Deregister)
		return;
	static const char* kLegacy[] = {
		"KB_HELPER_ACCOUNT", "KB_HELPER_TEKKIT", "KB_HELPER_MARKER_INTERACT",
		"KB_HELPER_EVENTS", "KB_HELPER_NOTES", "KB_HELPER_ITEM",
		"KB_HELPER_ECONOMY", "KB_HELPER_INSTANCES", "KB_HELPER_COMPLETION",
		"KB_HELPER_FARMING", "KB_HELPER_LOGS", "KB_HELPER_TRAILTOOLS",
		"KB_HELPER_COMPASS", "KB_HELPER_SETTINGS", "KB_HELPER_WALLET",
		"KB_HELPER_VAULT", "KB_HELPER_TPWATCH", "KB_HELPER_LOOKUP"
	};
	for (const char* id : kLegacy)
		G::API->InputBinds_Deregister(id);
}
