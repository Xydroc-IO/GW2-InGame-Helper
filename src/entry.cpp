#include <windows.h>
#include <cstdio>

#include "imgui/imgui.h"

#include "Globals.h"
#include "HelperQuickAccess.h"
#include "ItemLookup.h"
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
	char  LastQuery[128] = "";
	char  ActiveSiteId[64] = "home";
}

static constexpr const char* KB_TOGGLE = "KB_HELPER_TOGGLE";
static constexpr const char* KB_ITEM   = "KB_HELPER_ITEM";
static constexpr const char* WND_NAME  = "In-Game Helper##GW2InGameHelper";

static DWORD gLastToggleMs = 0;
static DWORD gLastItemMs = 0;

static void OnToggle(const char*, bool release)
{
	if (release)
		return;
	const DWORD now = GetTickCount();
	if (now - gLastToggleMs < 250)
		return;
	gLastToggleMs = now;

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

static void OnItemLookup(const char*, bool release)
{
	/* Fire on key-up so Ctrl/Shift are released before Shift+Click capture. */
	if (!release)
		return;
	const DWORD now = GetTickCount();
	if (now - gLastItemMs < 600)
		return;
	gLastItemMs = now;
	ItemLookup::LookupHoveredItem();
}

static unsigned CefModsFromWin()
{
	unsigned m = 0;
	if (GetKeyState(VK_SHIFT) & 0x8000)   m |= (1u << 1);
	if (GetKeyState(VK_CONTROL) & 0x8000) m |= (1u << 2);
	if (GetKeyState(VK_MENU) & 0x8000)    m |= (1u << 3);
	if (GetKeyState(VK_LBUTTON) & 0x8000) m |= (1u << 4);
	if (GetKeyState(VK_MBUTTON) & 0x8000) m |= (1u << 5);
	if (GetKeyState(VK_RBUTTON) & 0x8000) m |= (1u << 6);
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

/*
 * Return 0 = message consumed, do not pass to Guild Wars 2.
 * Same pattern as Gw2-InGame-Wiki: Nexus owns the binds; WndProc only
 * covers toggle while the browser has focus (Nexus often misses those).
 */
static UINT OnWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM)
{
	if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
	{
		const bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
		const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
		const bool alt   = (GetKeyState(VK_MENU) & 0x8000) != 0;
		/* Match Nexus default CTRL+SHIFT+H; K kept as wiki-style alternate. */
		if (ctrl && shift && !alt && (wp == 'H' || wp == 'K'))
		{
			OnToggle(KB_TOGGLE, false);
			return 0;
		}
		/* Item lookup: Nexus InputBinds only (fires on key-up). */
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
	/* Always start sessions on the how-to homepage. */
	Sites::SetActiveById("home");
	std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "home");
	G::ShowWiki = false;
	WikiBrowser::Init();
	ItemLookup::Init();

	api->GUI_Register(RT_Render, UI_Render);
	api->GUI_Register(RT_OptionsRender, UI_Options);
	api->GUI_RegisterCloseOnEscape(WND_NAME, &G::ShowWiki);

	api->InputBinds_RegisterWithString(KB_TOGGLE, OnToggle, "CTRL+SHIFT+H");
	api->InputBinds_RegisterWithString(KB_ITEM, OnItemLookup, "CTRL+SHIFT+I");
	api->WndProc_Register(OnWndProc);
	HelperQuickAccess::Init();

	api->Log(LOGL_INFO, ADDON_NAME,
		"Loaded — QuickAccess icon, Ctrl+Shift+H toggle, Ctrl+Shift+I item lookup.");
}

static void AddonUnload()
{
	if (!G::API)
		return;

	G::ShowWiki = false;
	G::API->GUI_Deregister(UI_Render);
	G::API->GUI_Deregister(UI_Options);
	G::API->GUI_DeregisterCloseOnEscape(WND_NAME);
	G::API->InputBinds_Deregister(KB_TOGGLE);
	G::API->InputBinds_Deregister(KB_ITEM);
	G::API->WndProc_Deregister(OnWndProc);

	ItemLookup::Shutdown();
	HelperQuickAccess::Shutdown();
	WikiBrowser::Shutdown();

	Settings::SetDirty();
	Settings::Save();

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
	G::AddonDef.Version.Minor    = 2;
	G::AddonDef.Version.Build    = 1;
	G::AddonDef.Version.Revision = 0;
	G::AddonDef.Author           = "xydroc";
	G::AddonDef.Description      =
		"Modular in-game browser. Ctrl+Shift+H toggle; Ctrl+Shift+I wiki item lookup.";
	G::AddonDef.Load             = AddonLoad;
	G::AddonDef.Unload           = AddonUnload;
	G::AddonDef.Flags            = AF_DisableHotloading;
	G::AddonDef.Provider         = UP_None;
	G::AddonDef.UpdateLink       = nullptr;
	return &G::AddonDef;
}
