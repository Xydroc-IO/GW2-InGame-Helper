#pragma once

#include <cstdint>

/* Shared helpers for WatchPad*.cpp (not public API). */
namespace WatchPadDetail
{
	void DrawHelp();
	void DrawWatchControls();
	void OpenMirror();
	void EnsureList();

	extern bool gRequestMirrorDock;
	extern int  gSelected;
	extern char gFilter[64];
}
