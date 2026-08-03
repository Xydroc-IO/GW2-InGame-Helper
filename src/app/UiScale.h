#pragma once

#include "Globals.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstring>

/* Per-window UI scale for our panels only.
   Never touch io.FontGlobalScale / style.ScaleAllSizes — Nexus shares ImGui.

   Effective scale = FontScale (Options slider) × window factor (vs design size).
   Window factor tracks the live window size so content grows/shrinks as the
   user resizes, clamped so tiny windows stay readable. */
namespace UiScale
{
	inline float Clampf(float v, float lo, float hi)
	{
		if (v < lo) return lo;
		if (v > hi) return hi;
		return v;
	}

	/* How much the current window differs from a design size (1.0 = design). */
	inline float WindowFactor(float refW = 560.f, float refH = 700.f)
	{
		const ImVec2 sz = ImGui::GetWindowSize();
		if (sz.x < 80.f || sz.y < 60.f || refW < 1.f || refH < 1.f)
			return 1.f;
		const float sx = sz.x / refW;
		const float sy = sz.y / refH;
		return Clampf(std::sqrt(sx * sy), 0.85f, 1.35f);
	}

	/* FontScale slider × window factor. Call after Begin(). */
	inline float EffectiveFontScale(float refW = 560.f, float refH = 700.f)
	{
		const float base = (G::FontScale > 0.1f) ? G::FontScale : 1.f;
		return Clampf(base * WindowFactor(refW, refH), 0.75f, 2.f);
	}

	/* Mild opt-in suggestion from display height only (no Nexus Scaling). */
	inline float Suggest(float /*displayW*/, float displayH)
	{
		float s = 1.f;
		if (displayH > 1600.f)
			s = Clampf(displayH / 1440.f, 1.f, 1.25f);
		return s;
	}

	inline void TickAuto()
	{
		if (!G::FontScaleAuto)
			return;
		const ImGuiIO& io = ImGui::GetIO();
		if (io.DisplaySize.x <= 100.f || io.DisplaySize.y <= 100.f)
			return;

		const float next = Suggest(io.DisplaySize.x, io.DisplaySize.y);
		if (std::fabs(next - G::FontScale) > 0.01f)
		{
			G::FontScale = next;
			Settings::SetDirty();
		}
	}

	/* Rail width from label text — not window size (that made rails huge). */
	inline float SideRailWidth(float design = 96.f, float /*refW*/ = 560.f, float /*refH*/ = 700.f)
	{
		const float base = (G::FontScale > 0.1f) ? G::FontScale : 1.f;
		return Clampf(design * base, 72.f, 200.f);
	}

	/* Widest visible label + frame/window padding (call after Begin + font scale). */
	inline float FitSideRailWidth(const char* const* labels, int count,
		float minW = 80.f, float maxW = 200.f)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		float w = minW;
		for (int i = 0; i < count; ++i)
		{
			if (!labels[i] || !labels[i][0])
				continue;
			const char* end = std::strstr(labels[i], "###");
			const ImVec2 ts = end
				? ImGui::CalcTextSize(labels[i], end, true)
				: ImGui::CalcTextSize(labels[i], nullptr, true);
			const float need = ts.x + style.FramePadding.x * 2.f +
				style.WindowPadding.x * 2.f + 6.f;
			if (need > w)
				w = need;
		}
		return Clampf(w, minW, maxW);
	}
}
