#pragma once

#include "AspectLayout.h"
#include "Globals.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstring>

/* Per-window UI scale for our panels only.
   Never touch io.FontGlobalScale / style.ScaleAllSizes — Nexus shares ImGui.

   Effective scale = FontScale (Options slider) × window factor (vs design size).
   Window factor tracks the live window size so content grows/shrinks as the
   user resizes, clamped so tiny windows stay readable.

   Display / aspect defaults (16:9 · 21:9 · 32:9) live in AspectLayout. */
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
		return Clampf(std::sqrt(sx * sy), 0.82f, 1.42f);
	}

	/* Options Font scale only — no per-window size multiplier.
	   (Window factor made small pads look tiny next to large ones.) */
	inline float EffectiveFontScale(float /*refW*/ = 560.f, float /*refH*/ = 700.f)
	{
		const float base = (G::FontScale > 0.1f) ? G::FontScale : 1.f;
		return Clampf(base, 0.75f, 2.f);
	}

	/* Opt-in suggestion — height + 16:9/21:9/32:9 awareness. */
	inline float Suggest(float displayW, float displayH)
	{
		return AspectLayout::SuggestFontScale(displayW, displayH);
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
		return Clampf(design * base, 72.f, 260.f);
	}

	/* Compact game-like icon dock (no labels). */
	inline float IconRailWidth(float iconSize = 26.f)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float base = Clampf((G::FontScale > 0.1f) ? G::FontScale : 1.f, 1.f, 1.25f);
		const float w = iconSize + style.FramePadding.x * 2.f + style.WindowPadding.x * 2.f + 12.f;
		return Clampf(w * base, 40.f, 64.f);
	}

	/* Widest visible label + frame/window padding (call after Begin + font scale).
	   iconSize > 0 reserves RailToggle icon + accent (Windows fonts often need more
	   than Linux — never hard-cap below measured need without raising maxW). */
	inline float FitSideRailWidth(const char* const* labels, int count,
		float minW = 80.f, float maxW = 260.f, float iconSize = 0.f)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float iconExtra = (iconSize > 0.f)
			? (iconSize + style.ItemInnerSpacing.x + style.FramePadding.x + 10.f)
			: 0.f;
		/* FontScale / denser Nexus fonts on some Windows hosts. */
		const float fontMul = Clampf((G::FontScale > 0.1f) ? G::FontScale : 1.f, 1.f, 1.75f);
		const float maxScaled = maxW * fontMul;
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
				style.WindowPadding.x * 2.f + 8.f + iconExtra;
			if (need > w)
				w = need;
		}
		return Clampf(w, minW, maxScaled > 320.f ? 320.f : maxScaled);
	}
}
