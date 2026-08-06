#pragma once

#include <cstdint>

#include <windows.h>

#include "Globals.h"

/* Shared entry / WndProc / hotkey state across entry*.cpp. */
namespace EntryDetail
{
	extern const char* KB_TOGGLE;

	extern DWORD gLastToggleMs;
	extern bool  gPollToggleHeld;
	extern bool  gSwallowHotkeyKeys;

	bool KeyDown(int vk);
	bool ModsCtrlShiftNoAlt();
	bool IsToggleVk(WPARAM wp);
	bool IsHotkeyChordVk(WPARAM wp);
	void BeginHotkeySwallow();
	void UpdateHotkeySwallow();
	void OnToggle(const char*, bool release);

	unsigned CefModsFromWin();
	bool IsKeyMsg(UINT msg);
	bool ClientCursor(HWND hwnd, int* outX, int* outY);

	enum : uint8_t { kKeyNone = 0, kKeyBrowser = 1, kKeyImGui = 2, kKeyGame = 3 };

	extern uint8_t sAteKeyDest[256];
	extern HWND sGameHwnd;

	LPARAM MakeKeyUpLParam(UINT vk);
	UINT OnWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
	void AddonLoad(AddonAPI_t* api);
	void AddonUnload();
}
