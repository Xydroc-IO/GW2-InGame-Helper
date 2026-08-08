#pragma once

#include <cstdint>

/* Shared helpers for WatchPad*.cpp (not public API). */
namespace WatchPadDetail
{
	void DrawHelp();
	void DrawWatchControls();
	void OpenMirror();
	void TickDeferredMirrorOpen();
	void EnsureList();

	extern bool gRequestMirrorDock;
	extern int  gDeferMirrorOpenFrames;
	extern int  gSelected;
	extern char gFilter[64];
}
