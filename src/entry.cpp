#include <windows.h>
#include <cstdio>

#include "imgui/imgui.h"

#include "Globals.h"
#include "HelperQuickAccess.h"
#include "Settings.h"
#include "Sites.h"
#include "UI.h"
#include "WikiBrowser.h"

namespace G
{
	AddonDefinition_t AddonDef{};
	AddonAPI_t*       API  = nullptr;
	HMODULE           Self = nullptr;

	bool  ShowWiki     = false;
	bool  ShowOptions  = true;
	float Opacity      = 0.97f;
	float FontScale    = 1.f;
	float WindowWidth  = 1100.f;
	float WindowHeight = 760.f;
	float WindowPosX   = 60.f;
	float WindowPosY   = 60.f;
	bool  HasSavedPos  = false;
	bool  KeepHelperWarm = false;
	char  LastQuery[128] = "";
	char  ActiveSiteId[64] = "home";
	char  DefaultSiteId[64] = "home";
}

static constexpr const char* KB_TOGGLE = "KB_HELPER_TOGGLE";
static constexpr const char* KB_ITEM_LEGACY = "KB_HELPER_ITEM"; /* removed — deregister only */

static DWORD gLastToggleMs = 0;
static bool  gPollToggleHeld = false;
static bool  gSwallowHotkeyKeys = false;

static bool KeyDown(int vk)
{
	return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static bool ModsCtrlShiftNoAlt()
{
	const bool ctrl  = KeyDown(VK_CONTROL) || KeyDown(VK_LCONTROL) || KeyDown(VK_RCONTROL);
	const bool shift = KeyDown(VK_SHIFT) || KeyDown(VK_LSHIFT) || KeyDown(VK_RSHIFT);
	const bool alt   = KeyDown(VK_MENU) || KeyDown(VK_LMENU) || KeyDown(VK_RMENU);
	return ctrl && shift && !alt;
}

static bool IsToggleVk(WPARAM wp)
{
	const unsigned v = static_cast<unsigned>(wp);
	return v == 'H' || v == 'K' || v == 'h' || v == 'k';
}

static bool IsHotkeyChordVk(WPARAM wp)
{
	const unsigned v = static_cast<unsigned>(wp);
	return IsToggleVk(wp) ||
		v == VK_CONTROL || v == VK_LCONTROL || v == VK_RCONTROL ||
		v == VK_SHIFT || v == VK_LSHIFT || v == VK_RSHIFT;
}

static void BeginHotkeySwallow()
{
	gSwallowHotkeyKeys = true;
}

static void UpdateHotkeySwallow()
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

static void OnToggle(const char*, bool release)
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

void HelperHotkeys_Poll()
{
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

static unsigned CefModsFromWin()
{
	unsigned m = 0;
	if (KeyDown(VK_SHIFT))   m |= (1u << 1);
	if (KeyDown(VK_CONTROL)) m |= (1u << 2);
	if (KeyDown(VK_MENU))    m |= (1u << 3);
	if (KeyDown(VK_LBUTTON)) m |= (1u << 4);
	if (KeyDown(VK_MBUTTON)) m |= (1u << 5);
	if (KeyDown(VK_RBUTTON)) m |= (1u << 6);
	return m;
}

static bool IsKeyMsg(UINT msg)
{
	switch (msg)
	{
	case WM_KEYDOWN: case WM_KEYUP:
	case WM_SYSKEYDOWN: case WM_SYSKEYUP:
	case WM_CHAR: case WM_SYSCHAR:
	case WM_DEADCHAR: case WM_SYSDEADCHAR:
		return true;
	default:
		return false;
	}
}

static bool ClientCursor(HWND hwnd, int* outX, int* outY)
{
	POINT pt{};
	if (!GetCursorPos(&pt))
		return false;
	if (hwnd && !ScreenToClient(hwnd, &pt))
		return false;
	*outX = pt.x;
	*outY = pt.y;
	return true;
}

static UINT OnWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM)
{
	UpdateHotkeySwallow();

	if (IsKeyMsg(msg))
	{
		const bool chordLetter = ModsCtrlShiftNoAlt() && IsToggleVk(wp);
		const bool swallowChordKey = gSwallowHotkeyKeys && IsHotkeyChordVk(wp);

		if (chordLetter || swallowChordKey)
		{
			if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && chordLetter)
				OnToggle(KB_TOGGLE, false);
			BeginHotkeySwallow();
			return 0;
		}
	}

	if (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN ||
		msg == WM_LBUTTONDBLCLK || msg == WM_RBUTTONDBLCLK || msg == WM_MBUTTONDBLCLK)
	{
		int cx = 0, cy = 0;
		if (UI_BlocksGameKeyboard() && ClientCursor(hwnd, &cx, &cy) &&
			!UI_IsPointerOverWiki(cx, cy))
		{
			UI_ReleaseGameInput();
		}
		return 1;
	}

	const bool blockKeys = UI_BlocksGameKeyboard();

	if (blockKeys && IsKeyMsg(msg))
	{
		if (ModsCtrlShiftNoAlt() && IsToggleVk(wp))
		{
			BeginHotkeySwallow();
			return 0;
		}

		const unsigned mods = CefModsFromWin();
		switch (msg)
		{
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			WikiBrowser::FeedKey(0, static_cast<int>(wp), mods, 0);
			break;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			WikiBrowser::FeedKey(2, static_cast<int>(wp), mods, 0);
			break;
		case WM_CHAR:
		case WM_SYSCHAR:
			if (wp >= 32 || wp == 8 || wp == 9 || wp == 13)
				WikiBrowser::FeedKey(3, static_cast<int>(wp), mods, static_cast<unsigned>(wp));
			break;
		default:
			break;
		}
		return 0;
	}

	return 1;
}

static void AddonLoad(AddonAPI_t* api)
{
	G::API = api;

	ImGui::SetCurrentContext(static_cast<ImGuiContext*>(api->ImguiContext));
	ImGui::SetAllocatorFunctions(
		reinterpret_cast<void* (*)(size_t, void*)>(api->ImguiMalloc),
		reinterpret_cast<void (*)(void*, void*)>(api->ImguiFree));

	Settings::Load();
	G::ShowWiki = false;
	gPollToggleHeld = false;
	gSwallowHotkeyKeys = false;
	WikiBrowser::Init();

	api->GUI_Register(RT_Render, UI_Render);
	api->GUI_Register(RT_OptionsRender, UI_Options);

	/* Drop legacy item-lookup bind so old Ctrl+Shift+I/U no longer fires. */
	api->InputBinds_Deregister(KB_ITEM_LEGACY);
	api->InputBinds_RegisterWithString(KB_TOGGLE, OnToggle, "CTRL+SHIFT+H");
	api->WndProc_Register(OnWndProc);
	HelperQuickAccess::Init();

	api->Log(LOGL_INFO, ADDON_NAME,
		"Loaded — Ctrl+Shift+H/K toggle (item lookup removed).");
}

static void AddonUnload()
{
	if (!G::API)
		return;

	G::ShowWiki = false;
	G::API->GUI_Deregister(UI_Render);
	G::API->GUI_Deregister(UI_Options);
	G::API->InputBinds_Deregister(KB_TOGGLE);
	G::API->InputBinds_Deregister(KB_ITEM_LEGACY);
	G::API->WndProc_Deregister(OnWndProc);

	HelperQuickAccess::Shutdown();
	WikiBrowser::Shutdown();

	Settings::SetDirty();
	Settings::Save(true);

	G::API = nullptr;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
		G::Self = hModule;
	return TRUE;
}

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
	G::AddonDef.Signature        = ADDON_SIG;
	G::AddonDef.APIVersion       = NEXUS_API_VERSION;
	G::AddonDef.Name             = ADDON_NAME;
	G::AddonDef.Version.Major    = 1;
	G::AddonDef.Version.Minor    = 7;
	G::AddonDef.Version.Build    = 2;
	G::AddonDef.Version.Revision = 1;
	G::AddonDef.Author           = "xydroc";
	G::AddonDef.Description      =
		"Modular in-game browser for GW2 sites and Discords. Ctrl+Shift+H toggle. One DLL, no memory reads.";
	G::AddonDef.Load             = AddonLoad;
	G::AddonDef.Unload           = AddonUnload;
	G::AddonDef.Flags            = AF_DisableHotloading;
	G::AddonDef.Provider         = UP_GitHub;
	G::AddonDef.UpdateLink       = "https://github.com/Xydroc-IO/GW2-InGame-Helper";
	return &G::AddonDef;
}
