#pragma once

#include "EiRuntime.h"

#include "imgui/imgui.h"

/* Wine crashes when side-rail opens kick CreateThread / SetNextWindowFocus /
   heavy fetch on the same ImGui click frame as Mirror + compass draw.
   Soft-open: show the pad first; run refresh a couple of frames later. */
namespace WinePadOpen
{
	inline bool Soft()
	{
		return EiRuntime::IsWine();
	}

	/* Frames to wait after Open before starting network/work. 0 = do it now. */
	inline int DeferFrames()
	{
		return Soft() ? 2 : 0;
	}

	inline void ApplyFocus(bool& focusFlag)
	{
		if (!focusFlag)
			return;
		focusFlag = false;
		if (!Soft())
			ImGui::SetNextWindowFocus();
	}

	/* Call once per Render while countdown > 0; returns true on the fire frame. */
	inline bool TickDefer(int& framesLeft)
	{
		if (framesLeft <= 0)
			return false;
		--framesLeft;
		return framesLeft == 0;
	}
}
