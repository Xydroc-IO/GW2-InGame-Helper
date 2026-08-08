#pragma once

#include "AspectLayout.h"
#include "Globals.h"

#include "imgui/imgui.h"
#include "imgui_internal.h"

#include <cmath>
#include <cstdio>

/* Place Notes / TP beside the main helper on first open. Persist user drags via
   G::PadGeom + Settings. Caller Place() only on the open frame (placeOnce). */
namespace PadDock
{
	/* Shared FirstUseEver defaults — same-tier pads open at matching footprints. */
	constexpr float kCompactW = 440.f;
	constexpr float kCompactH = 480.f;
	constexpr float kCompassH = 360.f; /* shorter: dial + short list */
	constexpr float kWorkbenchW = 560.f;
	constexpr float kWorkbenchH = 640.f;
	constexpr float kPathingW = 640.f;
	constexpr float kPathingH = 700.f;

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
		/* Skip title-strip height so minimize never poisons session dock size. */
		if (size.y < 80.f || size.x < 80.f)
			return;
		gNotes.x = pos.x;
		gNotes.y = pos.y;
		gNotes.w = size.x;
		gNotes.h = size.y;
		gNotes.valid = true;
	}

	inline void RememberTp(const ImVec2& pos, const ImVec2& size)
	{
		if (size.y < 80.f || size.x < 80.f)
			return;
		gTp.x = pos.x;
		gTp.y = pos.y;
		gTp.w = size.x;
		gTp.h = size.y;
		gTp.valid = true;
	}

	inline void ClearNotes() { gNotes = {}; }
	inline void ClearTp() { gTp = {}; }

	inline bool HasSavedPos(const G::PadGeom& g)
	{
		return g.x >= 0.f && g.y >= 0.f;
	}

	/* Resize ceilings — prefer most of the display so users can grow pads freely.
	   On 32:9, still allow wide pads but keep a sane floor. */
	inline float MaxW(float floorPx = 640.f)
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (io.DisplaySize.x <= 100.f)
			return floorPx > 1200.f ? floorPx : 1200.f;
		const AspectLayout::HelperGeom lim =
			AspectLayout::DefaultHelper(io.DisplaySize.x, io.DisplaySize.y);
		float fromDisp = io.DisplaySize.x * 0.94f;
		if (AspectLayout::Classify(io.DisplaySize.x, io.DisplaySize.y) ==
			AspectLayout::Class::Super_32_9)
			fromDisp = std::fmin(fromDisp, lim.maxW * 1.35f);
		return fromDisp > floorPx ? fromDisp : floorPx;
	}

	inline float MaxH(float floorPx = 480.f)
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (io.DisplaySize.y <= 100.f)
			return floorPx > 900.f ? floorPx : 900.f;
		const float fromDisp = io.DisplaySize.y * 0.94f;
		return fromDisp > floorPx ? fromDisp : floorPx;
	}

	/* Size constraints that allow the title-strip while custom-minimized.
	   Call BEFORE Begin with the same window name string as Begin(...). */
	inline void SetSizeConstraints(const char* windowName,
		float minW, float minH, float maxW, float maxH)
	{
		bool collapsed = false;
		if (windowName && windowName[0])
		{
			if (ImGuiWindow* w = ImGui::FindWindowByName(windowName))
				collapsed = w->StateStorage.GetBool(w->GetID("##gw2igh_pad_collapsed"), false);
		}
		const float useMinH = collapsed ? 28.f : minH;
		ImGui::SetNextWindowSizeConstraints(ImVec2(minW, useMinH), ImVec2(maxW, maxH));
	}

	inline ImVec2 ClampPos(float x, float y, float padW, float padH = 0.f)
	{
		const ImGuiIO& io = ImGui::GetIO();
		constexpr float kEdge = 8.f;
		/* Keep title bar (close/minimize) on-screen — not only the top-left corner. */
		constexpr float kTitleKeep = 72.f;
		if (io.DisplaySize.x > 100.f)
		{
			if (x + padW > io.DisplaySize.x - kEdge)
				x = io.DisplaySize.x - padW - kEdge;
			if (x < kEdge)
				x = kEdge;
		}
		if (io.DisplaySize.y > 100.f)
		{
			const float minVisible = padH > 0.f ? std::fmin(padH, kTitleKeep) : kTitleKeep;
			if (y + minVisible > io.DisplaySize.y - kEdge)
				y = io.DisplaySize.y - minVisible - kEdge;
			if (y < kEdge)
				y = kEdge;
		}
		return ImVec2(x, y);
	}

	/* After Begin — nudge if a saved geom left the close control off-screen. */
	inline void KeepOnScreen(float minVisibleH = 80.f)
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (io.DisplaySize.x <= 100.f || io.DisplaySize.y <= 100.f)
			return;
		const ImVec2 p = ImGui::GetWindowPos();
		const ImVec2 s = ImGui::GetWindowSize();
		const ImVec2 c = ClampPos(p.x, p.y, s.x, std::fmax(minVisibleH, std::fmin(s.y, 80.f)));
		if (std::fabs(c.x - p.x) > 0.5f || std::fabs(c.y - p.y) > 0.5f)
			ImGui::SetWindowPos(c);
	}

	inline ImVec2 BesideHelper(float padW)
	{
		const ImGuiIO& io = ImGui::GetIO();
		constexpr float kGap = 8.f;
		constexpr float kEdge = 8.f;

		if (!G::ShowWiki || G::WindowWidth < 80.f)
		{
			const float x = AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.55f);
			const float y = AspectLayout::PadFallbackY(io.DisplaySize.y, 0.12f);
			return ClampPos(x, y, padW);
		}

		float x = G::WindowPosX + G::WindowWidth + kGap;
		float y = G::WindowPosY;

		if (io.DisplaySize.x > 100.f && x + padW > io.DisplaySize.x - kEdge)
		{
			/* Left of the nav column, not on top of the icons. */
			const float railW = (G::SideRailW > 1.f) ? G::SideRailW : 64.f;
			x = G::WindowPosX - railW - padW - kGap;
			if (x < kEdge)
			{
				x = G::WindowPosX;
				y = G::WindowPosY + G::WindowHeight + kGap;
			}
		}

		return ClampPos(x, y, padW);
	}

	inline ImVec2 ForTrailPopout(float padW, float /*defH*/ = 560.f)
	{
		constexpr float kGap = 8.f;
		if (G::ShowTrailTools && HasSavedPos(G::PadTrailTools) && G::PadTrailTools.h >= 60.f)
			return ClampPos(G::PadTrailTools.x + G::PadTrailTools.w + kGap, G::PadTrailTools.y, padW);
		const ImVec2 base = BesideHelper(padW);
		return ClampPos(base.x + 24.f, base.y + 48.f, padW);
	}

	inline ImVec2 ForMarkerPopout(float padW, float /*defH*/ = 560.f)
	{
		constexpr float kGap = 8.f;
		if (HasSavedPos(G::PadTrailEditor) && G::PadTrailEditor.h >= 60.f)
			return ClampPos(G::PadTrailEditor.x, G::PadTrailEditor.y + G::PadTrailEditor.h + kGap, padW);
		if (G::ShowTrailTools && HasSavedPos(G::PadTrailTools) && G::PadTrailTools.h >= 60.f)
			return ClampPos(G::PadTrailTools.x + G::PadTrailTools.w + kGap,
				G::PadTrailTools.y + 80.f, padW);
		const ImVec2 base = BesideHelper(padW);
		return ClampPos(base.x + 48.f, base.y + 96.f, padW);
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
		/* Heal undersized saves so title close control isn't clipped at open. */
		constexpr float kMinUsefulW = 360.f;
		constexpr float kMinUsefulH = 200.f;
		float useW = (g.w >= 80.f) ? g.w : defW;
		float useH = (g.h >= 60.f) ? g.h : defH;
		if (useW < kMinUsefulW)
			useW = defW;
		if (useH < kMinUsefulH)
			useH = defH;
		if (HasSavedPos(g))
			ImGui::SetNextWindowPos(ClampPos(g.x, g.y, useW, useH), ImGuiCond_Always);
		else
			ImGui::SetNextWindowPos(fallbackPos, ImGuiCond_Always);
		if (applySize)
		{
			if (g.w >= 80.f && g.h >= 60.f && g.w >= kMinUsefulW && g.h >= kMinUsefulH)
				ImGui::SetNextWindowSize(ImVec2(g.w, g.h), ImGuiCond_Always);
			else
				ImGui::SetNextWindowSize(ImVec2(useW, useH), ImGuiCond_Always);
		}
		ImGui::SetNextWindowFocus();
		placeOnce = false;
	}

	/* After Begin — remember geom for settings.ini. Returns true if changed.
	   While custom-minimized, only persist position (keep pre-minimize size). */
	inline bool Capture(G::PadGeom& g)
	{
		const ImVec2 p = ImGui::GetWindowPos();
		const ImVec2 s = ImGui::GetWindowSize();
		const bool minimized = ImGui::GetStateStorage()->GetBool(
			ImGui::GetID("##gw2igh_pad_collapsed"), false);
		const float w = minimized ? g.w : s.x;
		const float h = minimized ? g.h : s.y;
		if (std::fabs(p.x - g.x) > 0.5f || std::fabs(p.y - g.y) > 0.5f ||
			std::fabs(w - g.w) > 0.5f || std::fabs(h - g.h) > 0.5f)
		{
			g.x = p.x;
			g.y = p.y;
			if (!minimized)
			{
				g.w = s.x;
				g.h = s.y;
			}
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
