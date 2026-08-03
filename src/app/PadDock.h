#pragma once

#include "Globals.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>

/* Place Notes / TP beside the main helper on first open. Persist user drags via
   G::PadGeom + Settings. Caller Place() only on the open frame (placeOnce). */
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

	inline bool HasSavedPos(const G::PadGeom& g)
	{
		return g.x >= 0.f && g.y >= 0.f;
	}

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

	/* Apply saved pos/size once on open; otherwise fallbackPos + defW/defH. */
	inline void Place(G::PadGeom& g, bool& placeOnce, float defW, float defH, ImVec2 fallbackPos,
		bool applySize = true)
	{
		if (!placeOnce)
			return;
		const float useW = (g.w >= 80.f) ? g.w : defW;
		if (HasSavedPos(g))
			ImGui::SetNextWindowPos(ClampPos(g.x, g.y, useW), ImGuiCond_Always);
		else
			ImGui::SetNextWindowPos(fallbackPos, ImGuiCond_Always);
		if (applySize)
		{
			if (g.w >= 80.f && g.h >= 60.f)
				ImGui::SetNextWindowSize(ImVec2(g.w, g.h), ImGuiCond_Always);
			else
				ImGui::SetNextWindowSize(ImVec2(defW, defH), ImGuiCond_Always);
		}
		ImGui::SetNextWindowFocus();
		placeOnce = false;
	}

	/* After Begin — remember geom for settings.ini. Returns true if changed. */
	inline bool Capture(G::PadGeom& g)
	{
		const ImVec2 p = ImGui::GetWindowPos();
		const ImVec2 s = ImGui::GetWindowSize();
		if (std::fabs(p.x - g.x) > 0.5f || std::fabs(p.y - g.y) > 0.5f ||
			std::fabs(s.x - g.w) > 0.5f || std::fabs(s.y - g.h) > 0.5f)
		{
			g.x = p.x;
			g.y = p.y;
			g.w = s.x;
			g.h = s.y;
			return true;
		}
		return false;
	}

	inline bool ParseGeom(const char* val, G::PadGeom& g)
	{
		if (!val || !val[0])
			return false;
		float x = -1.f, y = -1.f, w = 0.f, h = 0.f;
		if (std::sscanf(val, "%f,%f,%f,%f", &x, &y, &w, &h) < 2)
			return false;
		g.x = x;
		g.y = y;
		g.w = w;
		g.h = h;
		return true;
	}

	inline void WriteGeom(FILE* f, const char* key, const G::PadGeom& g)
	{
		if (!f || !key)
			return;
		std::fprintf(f, "%s=%.1f,%.1f,%.1f,%.1f\n", key, g.x, g.y, g.w, g.h);
	}
}
