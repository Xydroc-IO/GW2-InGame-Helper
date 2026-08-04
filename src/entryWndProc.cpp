#include "entryInternal.h"

#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

#include "imgui/imgui.h"

#include "Globals.h"
#include "UI.h"
#include "WikiBrowser.h"

using namespace EntryDetail;

namespace EntryDetail
{
unsigned CefModsFromWin()
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

bool IsKeyMsg(UINT msg)
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

bool ClientCursor(HWND hwnd, int* outX, int* outY)
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
uint8_t sAteKeyDest[256]{};
HWND sGameHwnd = nullptr;

LPARAM MakeKeyUpLParam(UINT vk)
{
	const UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
	/* repeat=1, scan, prev-down=1, up=1 */
	return static_cast<LPARAM>(1u | ((sc & 0xffu) << 16) | (1u << 30) | (1u << 31));
}
UINT OnWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
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

} // namespace EntryDetail

using namespace EntryDetail;

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
