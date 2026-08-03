#include <windows.h>
#include <cstdio>
#include <cstring>

#include "imgui/imgui.h"

#include "AccountPad.h"
#include "CharacterProfiles.h"
#include "Globals.h"
#include "HelperQuickAccess.h"
#include "LookupPad.h"
#include "NotesPad.h"
#include "TpWatchPad.h"
#include "EventsPad.h"
#include "TekkitGuidesPad.h"
#include "TekkitTrails.h"
#include "VaultPad.h"
#include "WalletPad.h"
#include "Settings.h"
#include "Sites.h"
#include "UI.h"
#include "WikiBrowser.h"

namespace G
{
	AddonDefinition_t AddonDef{};
	AddonAPI_t*       API       = nullptr;
	NexusLinkData_t*  NexusLink = nullptr;
	MumbleLinkedMem*  Mumble    = nullptr;
	HMODULE           Self      = nullptr;

	bool  ShowWiki     = false;
	bool  ShowOptions  = true;
	bool  ShowNotes    = false;
	bool  ShowTpWatch  = false;
	bool  ShowLookup   = false;
	bool  ShowWallet   = false;
	bool  ShowVault    = false;
	bool  ShowAccount  = false;
	bool  ShowEvents   = false;
	bool  ShowLogManager = false;
	bool  ShowTekkitGuides = false;
	bool  ShowTekkitTrails = true;
	bool  ShowCompassOverlay = true;
	bool  ShowWorldTrails = true;
	bool  ShowDirectionCompass = false;
	float DirectionLetterScale = 1.f;
	float DirectionWorldRadiusScale = 1.f;
	bool  HideWhenMapOpen = true;
	bool  HideOutOfGameplay = true;
	float WorldTrailMaxDist = 120.f;
	float WorldTrailWidth = 1.f;
	float Opacity      = 0.97f;
	float FontScale    = 1.f;
	float WindowWidth  = 1100.f;
	float WindowHeight = 760.f;
	float WindowPosX   = 60.f;
	float WindowPosY   = 60.f;
	bool  HasSavedPos  = false;
	bool  HasSavedSize = false;
	bool  KeepHelperWarm = false;
	char  LastQuery[128] = "";
	char  ActiveSiteId[64] = "home";
	char  DefaultSiteId[64] = "home";
	char  Gw2ApiKey[128] = "";
	char  TpWatchIds[1024] = "";
	char  TpWatchAlerts[2048] = "";
	char  EventTrackIds[4096] = "";
	char  TekkitEnabled[8192] = "";
	char  LogFolder[512] = "";
	char  EliteInsightsPath[512] = "";
	char  DpsReportToken[128] = "";
	float LogManagerListFrac = 0.55f;
	float LogManagerWinW = 1760.f;
	float LogManagerWinH = 900.f;
	float LogManagerWinX = -1.f;
	float LogManagerWinY = -1.f;
	bool  LogManagerGroupByEncounter = true;
}

static constexpr const char* KB_TOGGLE = "KB_HELPER_BETA_TOGGLE";
static constexpr const char* KB_ACCOUNT = "KB_HELPER_BETA_ACCOUNT";
static constexpr const char* KB_TEKKIT = "KB_HELPER_BETA_TEKKIT";
static constexpr const char* KB_EVENTS = "KB_HELPER_BETA_EVENTS";
static constexpr const char* KB_NOTES = "KB_HELPER_BETA_NOTES";
static constexpr const char* KB_ITEM_LEGACY = "KB_HELPER_BETA_ITEM"; /* removed — deregister only */

static DWORD gLastToggleMs = 0;
static DWORD gLastPanelBindMs = 0;
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

static bool PanelBindDebounce()
{
	const DWORD now = GetTickCount();
	if (now - gLastPanelBindMs < 250)
		return false;
	gLastPanelBindMs = now;
	return true;
}

static void OnToggleAccount(const char*, bool release)
{
	if (release || !PanelBindDebounce()) return;
	if (G::ShowAccount)
	{
		G::ShowAccount = false;
		Settings::SetDirty();
	}
	else
		AccountPad::OpenAndRefresh();
}

static void OnToggleTekkit(const char*, bool release)
{
	if (release || !PanelBindDebounce()) return;
	if (G::ShowTekkitGuides)
	{
		G::ShowTekkitGuides = false;
		Settings::SetDirty();
	}
	else
		TekkitGuidesPad::Open();
}

static void OnToggleEvents(const char*, bool release)
{
	if (release || !PanelBindDebounce()) return;
	if (G::ShowEvents)
	{
		G::ShowEvents = false;
		Settings::SetDirty();
	}
	else
		EventsPad::OpenAndRefresh();
}

static void OnToggleNotes(const char*, bool release)
{
	if (release || !PanelBindDebounce()) return;
	if (G::ShowNotes)
	{
		G::ShowNotes = false;
		Settings::SetDirty();
	}
	else
		NotesPad::Open();
}

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

/* Key routing: who ate KEYDOWN so KEYUP goes to the same place. */
enum : uint8_t { kKeyNone = 0, kKeyBrowser = 1, kKeyImGui = 2, kKeyGame = 3 };
static uint8_t sAteKeyDest[256]{};
static HWND sGameHwnd = nullptr;

static LPARAM MakeKeyUpLParam(UINT vk)
{
	const UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
	/* repeat=1, scan, prev-down=1, up=1 */
	return static_cast<LPARAM>(1u | ((sc & 0xffu) << 16) | (1u << 30) | (1u << 31));
}

void UI_ReleaseHeldGameKeys()
{
	if (!G::API || !G::API->WndProc_SendToGameOnly || !sGameHwnd)
		return;

	for (int vk = 1; vk < 256; ++vk)
	{
		if (sAteKeyDest[vk] == kKeyBrowser || sAteKeyDest[vk] == kKeyImGui)
			continue;

		const bool markedGame = sAteKeyDest[vk] == kKeyGame;
		const bool held = (GetAsyncKeyState(vk) & 0x8000) != 0;
		if (!markedGame && !held)
			continue;

		/* Skip mouse button VKs — movement stick is keyboard. */
		if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON ||
			vk == VK_XBUTTON1 || vk == VK_XBUTTON2)
			continue;

		G::API->WndProc_SendToGameOnly(
			sGameHwnd, WM_KEYUP, static_cast<WPARAM>(vk), MakeKeyUpLParam(static_cast<UINT>(vk)));
		sAteKeyDest[vk] = kKeyNone;
	}
}

void UI_ResetKeyRouting()
{
	std::memset(sAteKeyDest, 0, sizeof(sAteKeyDest));
	/* Nexus may deliver WndProc before ImGui is ready — never touch IO blindly. */
	if (!ImGui::GetCurrentContext())
		return;
	/* Drop only our CaptureKeyboardFromApp claim. Never clear WantTextInput /
	   KeysDown — the ImGui context is shared with Nexus; wiping them breaks
	   library search and other addons' text fields. */
	ImGui::CaptureKeyboardFromApp(false);
}

static UINT OnWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	sGameHwnd = hwnd;
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

	/* Mouse: Addon WndProcs run BEFORE Nexus UiInput (see Nexus Hooks.cpp).
	   Returning 0 without feeding ImGui skips MousePos → can't drag.
	   Eat MOVE when over the helper (after setting MousePos) so GW2 camera
	   look does not spin while the cursor is on the overlay. */
	if (msg == WM_MOUSEMOVE)
	{
		int cx = 0, cy = 0;
		const bool haveCursor = ClientCursor(hwnd, &cx, &cy);
		const bool overWiki = haveCursor && UI_IsPointerOverWiki(cx, cy);
		if (overWiki || UI_BlocksGameMouse())
		{
			if (haveCursor)
			{
				ImGuiIO& io = ImGui::GetIO();
				io.MousePos = ImVec2(static_cast<float>(cx), static_cast<float>(cy));
			}
			return 0; /* block game camera look */
		}
		return 1;
	}

	const bool mouseDown =
		msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN || msg == WM_MBUTTONDOWN ||
		msg == WM_LBUTTONDBLCLK || msg == WM_RBUTTONDBLCLK || msg == WM_MBUTTONDBLCLK ||
		msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK;
	const bool mouseUp =
		msg == WM_LBUTTONUP || msg == WM_RBUTTONUP || msg == WM_MBUTTONUP ||
		msg == WM_XBUTTONUP;
	const bool mouseWheel = msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL;
	if (mouseDown || mouseUp || mouseWheel)
	{
		int cx = 0, cy = 0;
		const bool haveCursor = ClientCursor(hwnd, &cx, &cy);
		const bool overWiki = haveCursor && UI_IsPointerOverWiki(cx, cy);
		const bool blockNow = overWiki || UI_BlocksGameMouse();

		/* Click outside while typing/focused → release so the game can take over. */
		if (mouseDown && UI_BlocksGameKeyboard() && haveCursor && !overWiki)
			UI_ReleaseGameInput();

		if (blockNow && mouseDown)
		{
			/* Flush WASD only on left-click into the helper (starting to use it).
			   Do NOT flush on hover or RMB — after camera-look the cursor often
			   reappears over the overlay and that used to KEYUP all movement. */
			if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK)
				UI_ReleaseHeldGameKeys();

			ImGuiIO& io = ImGui::GetIO();
			int button = 0;
			if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) button = 0;
			else if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) button = 1;
			else if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) button = 2;
			else if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK)
				button = (GET_XBUTTON_WPARAM(wp) == XBUTTON1) ? 3 : 4;
			io.MouseDown[button] = true;
			if (haveCursor)
				io.MousePos = ImVec2(static_cast<float>(cx), static_cast<float>(cy));
			return 0; /* block game skill/camera; ImGui already has the press */
		}

		if (blockNow && mouseWheel)
		{
			ImGuiIO& io = ImGui::GetIO();
			const float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) /
				static_cast<float>(WHEEL_DELTA);
			if (msg == WM_MOUSEWHEEL)
				io.MouseWheel += delta;
			else
				io.MouseWheelH += delta;
			return 0;
		}

		/* Button-ups: clear ImGui state but pass through (Nexus UiInput does the same). */
		if (mouseUp)
		{
			ImGuiIO& io = ImGui::GetIO();
			if (msg == WM_LBUTTONUP) io.MouseDown[0] = false;
			else if (msg == WM_RBUTTONUP) io.MouseDown[1] = false;
			else if (msg == WM_MBUTTONUP) io.MouseDown[2] = false;
			else if (msg == WM_XBUTTONUP)
				io.MouseDown[(GET_XBUTTON_WPARAM(wp) == XBUTTON1) ? 3 : 4] = false;
		}
		return 1;
	}

	/* Helper closed: pass keys to the game — unless Notes/TP (or similar) is
	   hovered and capturing. Never clear WantTextInput / KeysDown (shared ImGui). */
	if (IsKeyMsg(msg) && !G::ShowWiki && !UI_BlocksGameKeyboard())
	{
		const UINT vk = static_cast<UINT>(wp);
		const bool vkOk = vk < 256;
		const bool down = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
		const bool up = msg == WM_KEYUP || msg == WM_SYSKEYUP;
		if (vkOk)
		{
			if (sAteKeyDest[vk] == kKeyBrowser || sAteKeyDest[vk] == kKeyImGui)
				sAteKeyDest[vk] = kKeyNone;
			if (down)
				sAteKeyDest[vk] = kKeyGame;
			if (up)
				sAteKeyDest[vk] = kKeyNone;
		}
		return 1;
	}

	/* Helper open but not capturing (cursor on game/chat): same pass-through.
	   Do not honor stale CEF/ImGui ownership — that uniquely broke Space in chat
	   (ImGui Nav / active InputText kept eating VK_SPACE).
	   Leave WantTextInput alone so Nexus UI can still receive typing. */
	if (IsKeyMsg(msg) && G::ShowWiki && !UI_BlocksGameKeyboard())
	{
		const UINT vk = static_cast<UINT>(wp);
		const bool vkOk = vk < 256;
		const bool down = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
		const bool up = msg == WM_KEYUP || msg == WM_SYSKEYUP;
		if (vkOk)
		{
			if (sAteKeyDest[vk] == kKeyBrowser || sAteKeyDest[vk] == kKeyImGui)
				sAteKeyDest[vk] = kKeyNone;
			if (down)
				sAteKeyDest[vk] = kKeyGame;
			if (up)
				sAteKeyDest[vk] = kKeyNone;
		}
		return 1;
	}

	const bool blockKeys = UI_BlocksGameKeyboard();
	/* Keys we ate on KEYDOWN must also eat KEYUP even if focus flipped mid-press.
	   kKeyGame marks holds that went to GW2 — on helper hover we flush KEYUPs via
	   WndProc_SendToGameOnly so WASD does not stick. */

	if (IsKeyMsg(msg))
	{
		const UINT vk = static_cast<UINT>(wp);
		const bool vkOk = vk < 256;
		const uint8_t pending = vkOk ? sAteKeyDest[vk] : static_cast<uint8_t>(kKeyNone);
		const bool down = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
		const bool up = msg == WM_KEYUP || msg == WM_SYSKEYUP;
		/* lParam bit 30: key was already down (auto-repeat / held before we blocked). */
		const bool wasAlreadyDown = (static_cast<ULONG_PTR>(lp) & (1ull << 30)) != 0;

		auto sendKeyUpToGame = [&](UINT key) {
			if (G::API && G::API->WndProc_SendToGameOnly && sGameHwnd)
				G::API->WndProc_SendToGameOnly(sGameHwnd, WM_KEYUP, key, MakeKeyUpLParam(key));
		};

		auto passKeyToGame = [&]() -> UINT {
			if (ImGui::GetCurrentContext())
			{
				ImGuiIO& io = ImGui::GetIO();
				io.WantCaptureKeyboard = false;
				ImGui::CaptureKeyboardFromApp(false);
			}
			if (down && vkOk)
				sAteKeyDest[vk] = kKeyGame;
			if (up && vkOk)
				sAteKeyDest[vk] = kKeyNone;
			return 1;
		};

		/* CEF page owns typing — but game-owned holds must release to GW2, not CEF. */
		if (UI_BrowserKeyboardActive())
		{
			if (ModsCtrlShiftNoAlt() && IsToggleVk(wp))
			{
				BeginHotkeySwallow();
				if (vkOk)
					sAteKeyDest[vk] = kKeyNone;
				return 0;
			}

			const unsigned mods = CefModsFromWin();
			const int native = static_cast<int>(lp);
			const bool sys = (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || msg == WM_SYSCHAR);
			const bool gameOwned = pending == kKeyGame ||
				(pending == kKeyNone && (wasAlreadyDown || up));

			if (gameOwned && (down || up))
			{
				/* Stop stuck run: force KEYUP to game, do not feed movement into CEF. */
				if (vkOk)
				{
					sendKeyUpToGame(vk);
					sAteKeyDest[vk] = kKeyNone;
				}
				return 0;
			}

			switch (msg)
			{
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
				WikiBrowser::FeedKey(0, static_cast<int>(wp), mods, 0, native, sys);
				{
					BYTE state[256];
					if (GetKeyboardState(state))
					{
						WCHAR chars[8]{};
						const UINT sc = (static_cast<UINT>(lp) >> 16) & 0xffu;
						const int n = ToUnicode(static_cast<UINT>(wp), sc, state, chars, 8, 0);
						if (n > 0)
						{
							for (int i = 0; i < n; ++i)
							{
								const unsigned ch = static_cast<unsigned>(chars[i]);
								if (ch >= 32 || ch == 8 || ch == 9 || ch == 13)
									WikiBrowser::FeedKey(3, static_cast<int>(ch), mods, ch, native, sys);
							}
						}
					}
				}
				if (vkOk)
					sAteKeyDest[vk] = kKeyBrowser;
				break;
			case WM_KEYUP:
			case WM_SYSKEYUP:
				WikiBrowser::FeedKey(2, static_cast<int>(wp), mods, 0, native, sys);
				if (vkOk)
					sAteKeyDest[vk] = kKeyNone;
				break;
			default:
				break;
			}
			return 0;
		}

		/* Game still owns this key — never swallow the release. */
		if (up && (pending == kKeyNone || pending == kKeyGame))
			return passKeyToGame();

		/* Holding W, then hover chrome (not page): auto-repeat must not be stolen. */
		if (down && (pending == kKeyNone || pending == kKeyGame) && wasAlreadyDown)
			return passKeyToGame();

		if (blockKeys || pending != kKeyNone)
		{
			if (ModsCtrlShiftNoAlt() && IsToggleVk(wp))
			{
				BeginHotkeySwallow();
				if (vkOk)
					sAteKeyDest[vk] = kKeyNone;
				return 0;
			}

			const unsigned mods = CefModsFromWin();
			const int native = static_cast<int>(lp);
			const bool sys = (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || msg == WM_SYSCHAR);

			/* Prefer the sink that received KEYDOWN; on a fresh down use current focus. */
			const bool toBrowser = (pending == kKeyBrowser) ||
				(pending == kKeyNone && down && UI_BrowserKeyboardActive());

			if (toBrowser)
			{
				switch (msg)
				{
				case WM_KEYDOWN:
				case WM_SYSKEYDOWN:
					/* KEYEVENT_RAWKEYDOWN = 0 */
					WikiBrowser::FeedKey(0, static_cast<int>(wp), mods, 0, native, sys);
					/* Nexus often skips TranslateMessage when we return 0, so WM_CHAR
					   never arrives — synthesize characters here (fixes dropped letters). */
					{
						BYTE state[256];
						if (GetKeyboardState(state))
						{
							WCHAR chars[8]{};
							const UINT sc = (static_cast<UINT>(lp) >> 16) & 0xffu;
							const int n = ToUnicode(static_cast<UINT>(wp), sc, state, chars, 8, 0);
							if (n > 0)
							{
								for (int i = 0; i < n; ++i)
								{
									const unsigned ch = static_cast<unsigned>(chars[i]);
									if (ch >= 32 || ch == 8 || ch == 9 || ch == 13)
										WikiBrowser::FeedKey(3, static_cast<int>(ch), mods, ch, native, sys);
								}
							}
						}
					}
					if (vkOk)
						sAteKeyDest[vk] = kKeyBrowser;
					break;
				case WM_KEYUP:
				case WM_SYSKEYUP:
					/* KEYEVENT_KEYUP = 2 */
					WikiBrowser::FeedKey(2, static_cast<int>(wp), mods, 0, native, sys);
					if (vkOk)
						sAteKeyDest[vk] = kKeyNone;
					break;
				case WM_CHAR:
				case WM_SYSCHAR:
					/* Ignore — characters already sent from KEYDOWN via ToUnicode. */
					break;
				default:
					break;
				}
				return 0;
			}

			/* ImGui chrome (Browse filter / toolbar Search / Find) — feed ImGui
			   ourselves and eat the message so GW2 never sees it (autorun "R"). */
			ImGuiIO& io = ImGui::GetIO();
			switch (msg)
			{
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
				if (vkOk)
				{
					io.KeysDown[vk] = true;
					sAteKeyDest[vk] = kKeyImGui;
				}
				{
					BYTE state[256];
					if (GetKeyboardState(state))
					{
						WCHAR chars[8]{};
						const UINT sc = (static_cast<UINT>(lp) >> 16) & 0xffu;
						const int n = ToUnicode(static_cast<UINT>(wp), sc, state, chars, 8, 0);
						if (n > 0)
						{
							for (int i = 0; i < n; ++i)
							{
								const unsigned ch = static_cast<unsigned>(chars[i]);
								if (ch >= 32 || ch == 8 || ch == 9 || ch == 13)
									io.AddInputCharacter(ch);
							}
						}
					}
				}
				break;
			case WM_KEYUP:
			case WM_SYSKEYUP:
				if (vkOk)
				{
					io.KeysDown[vk] = false;
					sAteKeyDest[vk] = kKeyNone;
				}
				break;
			case WM_CHAR:
			case WM_SYSCHAR:
				break;
			default:
				break;
			}
			return 0;
		}

		/* Key goes to the game — remember so hover can flush KEYUP. */
		if (down && vkOk)
			sAteKeyDest[vk] = kKeyGame;
		if (up && vkOk)
			sAteKeyDest[vk] = kKeyNone;
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

	G::NexusLink = static_cast<NexusLinkData_t*>(api->DataLink_Get(DL_NEXUS_LINK));
	G::Mumble = static_cast<MumbleLinkedMem*>(api->DataLink_Get(DL_MUMBLE_LINK));

	Settings::Load();
	NotesPad::Load();
	CharacterProfiles::Load();
	TekkitTrails::Init();
	/* Restore category toggles after Init (Init no longer wipes them, but first
	   load applies settings here so order stays Load → Init → apply). */
	if (G::TekkitEnabled[0])
		TekkitTrails::ParseEnabledPaths(G::TekkitEnabled);
	G::ShowWiki = false;
	G::ShowNotes = false;
	G::ShowTpWatch = false;
	G::ShowLookup = false;
	G::ShowWallet = false;
	G::ShowVault = false;
	G::ShowAccount = false;
	G::ShowEvents = false;
	G::ShowLogManager = false;
	G::ShowTekkitGuides = false;
	gPollToggleHeld = false;
	gSwallowHotkeyKeys = false;
	Sites::Init();
	WikiBrowser::Init();

	api->GUI_Register(RT_Render, UI_Render);
	api->GUI_Register(RT_OptionsRender, UI_Options);

	/* Drop legacy item-lookup bind so old Ctrl+Shift+I/U no longer fires. */
	api->InputBinds_Deregister(KB_ITEM_LEGACY);
	api->InputBinds_RegisterWithString(KB_TOGGLE, OnToggle, "CTRL+SHIFT+H");
	/* Panel pads — rebind in Nexus Options → Keybinds. */
	api->InputBinds_RegisterWithString(KB_ACCOUNT, OnToggleAccount, "CTRL+SHIFT+A");
	api->InputBinds_RegisterWithString(KB_TEKKIT, OnToggleTekkit, "CTRL+SHIFT+G");
	api->InputBinds_RegisterWithString(KB_EVENTS, OnToggleEvents, "CTRL+SHIFT+E");
	api->InputBinds_RegisterWithString(KB_NOTES, OnToggleNotes, "CTRL+SHIFT+N");
	api->WndProc_Register(OnWndProc);
	HelperQuickAccess::Init();

	api->Log(LOGL_INFO, ADDON_NAME,
		"Loaded — Ctrl+Shift+H/K helper; A/G/E/N panels (rebind in Nexus).");
}

static void AddonUnload()
{
	if (!G::API)
		return;

	G::ShowWiki = false;
	G::API->GUI_Deregister(UI_Render);
	G::API->GUI_Deregister(UI_Options);
	G::API->InputBinds_Deregister(KB_TOGGLE);
	G::API->InputBinds_Deregister(KB_ACCOUNT);
	G::API->InputBinds_Deregister(KB_TEKKIT);
	G::API->InputBinds_Deregister(KB_EVENTS);
	G::API->InputBinds_Deregister(KB_NOTES);
	G::API->InputBinds_Deregister(KB_ITEM_LEGACY);
	G::API->WndProc_Deregister(OnWndProc);

	HelperQuickAccess::Shutdown();
	WikiBrowser::Shutdown();
	Sites::Shutdown();
	TekkitTrails::Shutdown();

	NotesPad::Save(true);
	CharacterProfiles::CaptureCurrent();
	CharacterProfiles::Save(true);
	Settings::SetDirty();
	Settings::Save(true);

	G::API = nullptr;
	G::NexusLink = nullptr;
	G::Mumble = nullptr;
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
	G::AddonDef.Version.Major    = 2;
	G::AddonDef.Version.Minor    = 1;
	G::AddonDef.Version.Build    = 0;
	G::AddonDef.Version.Revision = 4;
	G::AddonDef.Author           = "xydroc";
	G::AddonDef.Description      =
		"BETA — experimental In-Game Helper (routing + character profiles). "
		"Local builds do not auto-update from GitHub.";
	G::AddonDef.Load             = AddonLoad;
	G::AddonDef.Unload           = AddonUnload;
	G::AddonDef.Flags            = AF_DisableHotloading; /* CEF helper — no Nexus hot-reload */
	/* UP_None: local Beta installs must not be overwritten by GitHub releases. */
	G::AddonDef.Provider         = UP_None;
	G::AddonDef.UpdateLink       = nullptr;
	return &G::AddonDef;
}
