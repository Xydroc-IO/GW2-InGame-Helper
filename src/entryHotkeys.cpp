#include "entryInternal.h"

#include <windows.h>

#include "Globals.h"
#include "Settings.h"
#include "UI.h"
#include "WikiBrowser.h"

using namespace EntryDetail;

namespace EntryDetail
{
const char* KB_TOGGLE = "KB_HELPER_TOGGLE";

DWORD gLastToggleMs = 0;
bool  gPollToggleHeld = false;
bool  gSwallowHotkeyKeys = false;

bool KeyDown(int vk)
{
	return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

bool ModsCtrlShiftNoAlt()
{
	const bool ctrl  = KeyDown(VK_CONTROL) || KeyDown(VK_LCONTROL) || KeyDown(VK_RCONTROL);
	const bool shift = KeyDown(VK_SHIFT) || KeyDown(VK_LSHIFT) || KeyDown(VK_RSHIFT);
	const bool alt   = KeyDown(VK_MENU) || KeyDown(VK_LMENU) || KeyDown(VK_RMENU);
	return ctrl && shift && !alt;
}

bool IsToggleVk(WPARAM wp)
{
	const unsigned v = static_cast<unsigned>(wp);
	return v == 'H' || v == 'K' || v == 'h' || v == 'k';
}

bool IsHotkeyChordVk(WPARAM wp)
{
	const unsigned v = static_cast<unsigned>(wp);
	return IsToggleVk(wp) ||
		v == VK_CONTROL || v == VK_LCONTROL || v == VK_RCONTROL ||
		v == VK_SHIFT || v == VK_LSHIFT || v == VK_RSHIFT;
}

void BeginHotkeySwallow()
{
	gSwallowHotkeyKeys = true;
}

void UpdateHotkeySwallow()
{
	if (!gSwallowHotkeyKeys)
		return;
	if (!KeyDown(VK_CONTROL) && !KeyDown(VK_LCONTROL) && !KeyDown(VK_RCONTROL) &&
		!KeyDown(VK_SHIFT) && !KeyDown(VK_LSHIFT) && !KeyDown(VK_RSHIFT) &&
		!KeyDown('H') && !KeyDown('K'))
	{
		gSwallowHotkeyKeys = false;
	}
}

void OnToggle(const char*, bool release)
{
	if (release)
		return;
	const DWORD now = GetTickCount();
	if (now - gLastToggleMs < 250)
		return;
	gLastToggleMs = now;
	BeginHotkeySwallow();

	G::ShowWiki = !G::ShowWiki;
	if (!G::ShowWiki)
	{
		WikiBrowser::SetVisible(false);
		UI_ReleaseGameInput();
	}
	Settings::SetDirty();
	if (G::API && G::API->Log)
		G::API->Log(LOGL_INFO, ADDON_NAME, G::ShowWiki ? "Helper opened" : "Helper closed");
}
} // namespace EntryDetail

void HelperHotkeys_Poll()
{
	/* WndProc already handles the chord; this is a fallback. Cap ~30 Hz when
	   closed so every Nexus render tick is not a GetAsyncKeyState storm. */
	static DWORD sLastPollMs = 0;
	const DWORD now = GetTickCount();
	if (!G::ShowWiki && !gSwallowHotkeyKeys &&
		sLastPollMs != 0 && (now - sLastPollMs) < 33u)
		return;
	sLastPollMs = now;

	UpdateHotkeySwallow();

	const bool mods = ModsCtrlShiftNoAlt();
	const bool toggleKey = KeyDown('H') || KeyDown('K');
	const bool toggleDown = mods && toggleKey;
	if (toggleDown && !gPollToggleHeld)
		OnToggle(KB_TOGGLE, false);
	gPollToggleHeld = toggleDown;

	if (mods && toggleKey)
		BeginHotkeySwallow();
}
