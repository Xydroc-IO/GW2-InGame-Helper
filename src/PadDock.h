#pragma once

#include "Globals.h"

#include "imgui/imgui.h"

/* Place Notes / TP beside the main helper on open. If the other pad is already
   open, stack the new one directly below it (same X) so they never overlap.
   Caller uses ImGuiCond_Always only for that frame — afterward the user can drag. */
namespace PadDock
{
	struct Rect
	{
		float x = 0.f;
		float y = 0.f;
		float w = 0.f;
		float h = 0.f;
		bool  valid = false;
	};

	inline Rect gNotes{};
	inline Rect gTp{};

	inline void RememberNotes(const ImVec2& pos, const ImVec2& size)
	{
		gNotes.x = pos.x;
		gNotes.y = pos.y;
		gNotes.w = size.x;
		gNotes.h = size.y;
		gNotes.valid = size.x > 1.f && size.y > 1.f;
	}

	inline void RememberTp(const ImVec2& pos, const ImVec2& size)
	{
		gTp.x = pos.x;
		gTp.y = pos.y;
		gTp.w = size.x;
		gTp.h = size.y;
		gTp.valid = size.x > 1.f && size.y > 1.f;
	}

	inline void ClearNotes() { gNotes = {}; }
	inline void ClearTp() { gTp = {}; }

	inline ImVec2 ClampPos(float x, float y, float padW)
	{
		const ImGuiIO& io = ImGui::GetIO();
		constexpr float kEdge = 8.f;
		if (io.DisplaySize.x > 100.f)
		{
			if (x + padW > io.DisplaySize.x - kEdge)
				x = io.DisplaySize.x - padW - kEdge;
			if (x < kEdge)
				x = kEdge;
		}
		if (io.DisplaySize.y > 100.f)
		{
			if (y > io.DisplaySize.y - 48.f)
				y = io.DisplaySize.y - 48.f;
			if (y < kEdge)
				y = kEdge;
		}
		return ImVec2(x, y);
	}

	inline ImVec2 BesideHelper(float padW)
	{
		const ImGuiIO& io = ImGui::GetIO();
		constexpr float kGap = 8.f;
		constexpr float kEdge = 8.f;

		if (!G::ShowWiki || G::WindowWidth < 80.f)
		{
			const float x = (io.DisplaySize.x > 100.f) ? io.DisplaySize.x * 0.55f : 80.f;
			const float y = (io.DisplaySize.y > 100.f) ? io.DisplaySize.y * 0.12f : 80.f;
			return ClampPos(x, y, padW);
		}

		float x = G::WindowPosX + G::WindowWidth + kGap;
		float y = G::WindowPosY;

		if (io.DisplaySize.x > 100.f && x + padW > io.DisplaySize.x - kEdge)
		{
			x = G::WindowPosX - padW - kGap;
			if (x < kEdge)
			{
				x = G::WindowPosX;
				y = G::WindowPosY + G::WindowHeight + kGap;
			}
		}

		return ClampPos(x, y, padW);
	}

	/* Open Notes: below TP if TP is showing, else beside the helper. */
	inline ImVec2 ForNotes(float padW, float fallbackOtherH = 320.f)
	{
		constexpr float kGap = 8.f;
		if (G::ShowTpWatch)
		{
			if (gTp.valid)
				return ClampPos(gTp.x, gTp.y + gTp.h + kGap, padW);
			const ImVec2 base = BesideHelper(padW);
			return ClampPos(base.x, base.y + fallbackOtherH + kGap, padW);
		}
		return BesideHelper(padW);
	}

	/* Open TP: below Notes if Notes is showing, else beside the helper. */
	inline ImVec2 ForTp(float padW, float fallbackOtherH = 480.f)
	{
		constexpr float kGap = 8.f;
		if (G::ShowNotes)
		{
			if (gNotes.valid)
				return ClampPos(gNotes.x, gNotes.y + gNotes.h + kGap, padW);
			const ImVec2 base = BesideHelper(padW);
			return ClampPos(base.x, base.y + fallbackOtherH + kGap, padW);
		}
		return BesideHelper(padW);
	}
}
