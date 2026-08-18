#include "Gw2Ui.h"
#include "Gw2UiInternal.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "nexus/Nexus.h"

#include <algorithm>

namespace
{
	/* DAT ids from Pictures/Textures/UI Textures — named files in ui-chrome pack. */
	constexpr int kScrollThumbId = 154971; /* grip with hash marks */
	constexpr int kScrollMidId = 154970;   /* tileable mid strip */
	constexpr int kScrollTopId = 154969;   /* thumb top end */
	constexpr int kScrollCapId = 154973;   /* thumb bottom end */
	constexpr int kScrollArrowId = 155031; /* down chevron */

	constexpr ImU32 kBeige = IM_COL32(192, 186, 148, 255);
	constexpr ImU32 kBeigeDim = IM_COL32(168, 160, 128, 255);

	Texture_t* ScrollTex(const char* stem, int assetId)
	{
		Texture_t* t = Gw2UiDetail::GetChromeNamed(stem);
		if (t && t->Resource)
			return t;
		return Gw2UiDetail::GetChromeTex(assetId);
	}

	bool WindowUnderRoot(ImGuiWindow* window, ImGuiWindow* root)
	{
		if (!window || !root)
			return false;
		for (ImGuiWindow* w = window; w; w = w->ParentWindow)
		{
			if (w == root)
				return true;
		}
		return false;
	}

	ImU32 WithAlpha(ImU32 rgb, float a)
	{
		const int aa = static_cast<int>(a * (rgb >> 24) + 0.5f);
		return (rgb & 0x00FFFFFFu) | (static_cast<ImU32>(aa) << 24);
	}

	void DrawChevron(ImDrawList* dl, ImVec2 c, float half, bool up, ImU32 col, float thick)
	{
		const float yTip = up ? (c.y - half * 0.55f) : (c.y + half * 0.55f);
		const float yBase = up ? (c.y + half * 0.35f) : (c.y - half * 0.35f);
		dl->AddLine(ImVec2(c.x - half * 0.75f, yBase), ImVec2(c.x, yTip), col, thick);
		dl->AddLine(ImVec2(c.x + half * 0.75f, yBase), ImVec2(c.x, yTip), col, thick);
	}

	void Blit(ImDrawList* dl, Texture_t* tex, ImVec2 p0, ImVec2 p1, bool flipV, ImU32 tint)
	{
		if (!tex || !tex->Resource)
			return;
		const ImVec2 uv0(0.f, flipV ? 1.f : 0.f);
		const ImVec2 uv1(1.f, flipV ? 0.f : 1.f);
		dl->AddImage(reinterpret_cast<ImTextureID>(tex->Resource), p0, p1, uv0, uv1, tint);
	}

	/* Center a square DAT sprite inside a slot (keeps chevron proportions). */
	void BlitCentered(ImDrawList* dl, Texture_t* tex, ImVec2 slot0, ImVec2 slot1, bool flipV, ImU32 tint)
	{
		if (!tex || !tex->Resource)
			return;
		const float sw = slot1.x - slot0.x;
		const float sh = slot1.y - slot0.y;
		const float side = (std::min)(sw, sh);
		const float cx = (slot0.x + slot1.x) * 0.5f;
		const float cy = (slot0.y + slot1.y) * 0.5f;
		Blit(dl, tex, ImVec2(cx - side * 0.5f, cy - side * 0.5f),
			ImVec2(cx + side * 0.5f, cy + side * 0.5f), flipV, tint);
	}
}

void Gw2Ui::PaintNativeScrollbars(float opacity, ImGuiWindow* root)
{
	if (!root)
		root = ImGui::GetCurrentWindow();
	if (!root || root->Collapsed)
		return;

	float a = opacity;
	if (a < 0.f) a = 0.f;
	if (a > 1.f) a = 1.f;
	const ImU32 tint = IM_COL32(255, 255, 255, static_cast<int>(a * 255.f + 0.5f));
	const ImU32 cover = IM_COL32(18, 18, 18, static_cast<int>(a * 255.f + 0.5f));
	const ImU32 grabCol = WithAlpha(kBeige, a);
	const ImU32 grabEdge = WithAlpha(kBeigeDim, a);
	const ImU32 chevronCol = WithAlpha(kBeige, a);

	Texture_t* thumb = ScrollTex("scroll-thumb", kScrollThumbId);
	Texture_t* mid = ScrollTex("scroll-thumb-mid", kScrollMidId);
	Texture_t* top = ScrollTex("scroll-thumb-top", kScrollTopId);
	Texture_t* cap = ScrollTex("scroll-thumb-cap", kScrollCapId);
	Texture_t* arrowDown = ScrollTex("scroll-arrow", kScrollArrowId);
	Texture_t* arrowUp = Gw2UiDetail::GetChromeNamed("scroll-arrow-up");
	if (!arrowUp || !arrowUp->Resource)
		arrowUp = arrowDown;

	auto drawAxis = [&](ImGuiWindow* window, ImGuiAxis axis) {
		if (axis == ImGuiAxis_Y && !window->ScrollbarY)
			return;
		if (axis == ImGuiAxis_X && !window->ScrollbarX)
			return;

		/* Per-window draw list — keeps Z-order so other pads can cover this gutter. */
		ImDrawList* dl = window->DrawList;
		if (!dl)
			return;

		const ImRect bbFrame = ImGui::GetWindowScrollbarRect(window, axis);
		if (bbFrame.GetWidth() < 2.f || bbFrame.GetHeight() < 2.f)
			return;

		/* Match imgui_widgets.cpp ScrollbarEx grab so DAT thumb tracks the real hit box.
		   Inventing arrow gutters used to offset the thumb from the drag target. */
		ImRect bb = bbFrame;
		bb.Expand(ImVec2(
			-ImClamp(IM_FLOOR((bbFrame.GetWidth() - 2.f) * 0.5f), 0.f, 3.f),
			-ImClamp(IM_FLOOR((bbFrame.GetHeight() - 2.f) * 0.5f), 0.f, 3.f)));
		const float sizeAvail = window->InnerRect.Max[axis] - window->InnerRect.Min[axis];
		const float sizeContents = window->ContentSize[axis] + window->WindowPadding[axis] * 2.f;
		const float winSizeV = ImMax(ImMax(sizeContents, sizeAvail), 1.f);
		const float trackV = (axis == ImGuiAxis_X) ? bb.GetWidth() : bb.GetHeight();
		const float grabPx = ImClamp(trackV * (sizeAvail / winSizeV),
			ImGui::GetStyle().GrabMinSize, trackV);
		const float scrollMax = ImMax(1.f, sizeContents - sizeAvail);
		const float scroll = (axis == ImGuiAxis_Y) ? window->Scroll.y : window->Scroll.x;
		const float scrollRatio = ImSaturate(scroll / scrollMax);
		const float grabOffNorm = scrollRatio * (trackV - grabPx) / trackV;

		dl->PushClipRect(bbFrame.Min, bbFrame.Max, /*intersect_with_current=*/false);
		dl->AddRectFilled(bbFrame.Min, bbFrame.Max, cover);

		ImVec2 g0, g1;
		if (axis == ImGuiAxis_Y)
		{
			g0 = ImVec2(bb.Min.x, bb.Min.y + grabOffNorm * trackV);
			g1 = ImVec2(bb.Max.x, g0.y + grabPx);
		}
		else
		{
			g0 = ImVec2(bb.Min.x + grabOffNorm * trackV, bb.Min.y);
			g1 = ImVec2(g0.x + grabPx, bb.Max.y);
		}

		const float size = (axis == ImGuiAxis_Y) ? bbFrame.GetWidth() : bbFrame.GetHeight();
		const float grabLen = (axis == ImGuiAxis_Y) ? (g1.y - g0.y) : (g1.x - g0.x);
		const float trackLen = (axis == ImGuiAxis_Y) ? bbFrame.GetHeight() : bbFrame.GetWidth();

		if (axis == ImGuiAxis_Y)
		{
			/* Chevrons only when the grab leaves room — they are decoration, not hit targets. */
			if (grabLen < trackLen - size * 2.2f)
			{
				const ImVec2 top0(bbFrame.Min.x, bbFrame.Min.y);
				const ImVec2 top1(bbFrame.Max.x, bbFrame.Min.y + size);
				const ImVec2 bot0(bbFrame.Min.x, bbFrame.Max.y - size);
				const ImVec2 bot1(bbFrame.Max.x, bbFrame.Max.y);
				if (arrowUp && arrowUp->Resource)
					BlitCentered(dl, arrowUp, top0, top1, false, tint);
				else if (arrowDown && arrowDown->Resource)
					BlitCentered(dl, arrowDown, top0, top1, true, tint);
				else
					DrawChevron(dl, ImVec2((top0.x + top1.x) * 0.5f, (top0.y + top1.y) * 0.5f),
						size * 0.42f, true, chevronCol, 1.6f);
				if (arrowDown && arrowDown->Resource)
					BlitCentered(dl, arrowDown, bot0, bot1, false, tint);
				else
					DrawChevron(dl, ImVec2((bot0.x + bot1.x) * 0.5f, (bot0.y + bot1.y) * 0.5f),
						size * 0.42f, false, chevronCol, 1.6f);
			}

			const float endH = size * 0.55f;
			Texture_t* body = (mid && mid->Resource) ? mid : thumb;
			Texture_t* topCap = (top && top->Resource) ? top : cap;
			Texture_t* botCap = (cap && cap->Resource) ? cap : top;

			if (body && body->Resource && grabLen > endH * 2.4f &&
				((topCap && topCap->Resource) || (botCap && botCap->Resource)))
			{
				if (topCap && topCap->Resource)
					Blit(dl, topCap, ImVec2(g0.x, g0.y), ImVec2(g1.x, g0.y + endH), false, tint);
				else
					Blit(dl, body, ImVec2(g0.x, g0.y), ImVec2(g1.x, g0.y + endH), false, tint);

				const float mid0 = g0.y + endH * 0.85f;
				const float mid1 = g1.y - endH * 0.85f;
				Blit(dl, body, ImVec2(g0.x, mid0), ImVec2(g1.x, mid1), false, tint);
				if (thumb && thumb->Resource && (mid1 - mid0) > size * 1.2f)
				{
					const float gh = size * 1.1f;
					const float gcy = (mid0 + mid1) * 0.5f;
					Blit(dl, thumb, ImVec2(g0.x, gcy - gh * 0.5f),
						ImVec2(g1.x, gcy + gh * 0.5f), false, tint);
				}

				if (botCap && botCap->Resource)
					Blit(dl, botCap, ImVec2(g0.x, g1.y - endH), ImVec2(g1.x, g1.y), false, tint);
				else
					Blit(dl, body, ImVec2(g0.x, g1.y - endH), ImVec2(g1.x, g1.y), false, tint);
			}
			else if (thumb && thumb->Resource)
			{
				Blit(dl, thumb, g0, g1, false, tint);
			}
			else if (body && body->Resource)
			{
				Blit(dl, body, g0, g1, false, tint);
			}
			else
			{
				dl->AddRectFilled(g0, g1, grabCol);
				dl->AddRect(g0, g1, grabEdge);
			}
		}
		else
		{
			Texture_t* body = (mid && mid->Resource) ? mid : thumb;
			if (body && body->Resource)
				Blit(dl, body, g0, g1, false, tint);
			else
				dl->AddRectFilled(g0, g1, grabCol);
		}

		if (dl->_ClipRectStack.Size > 0)
			dl->PopClipRect();
	};

	auto paintWindow = [&](ImGuiWindow* window) {
		if (!window || window->Collapsed)
			return;
		if (!(window->Active || window->WasActive))
			return;
		drawAxis(window, ImGuiAxis_Y);
		drawAxis(window, ImGuiAxis_X);
	};

	paintWindow(root);

	ImGuiContext& g = *GImGui;
	for (int i = 0; i < g.Windows.Size; ++i)
	{
		ImGuiWindow* w = g.Windows[i];
		if (w == root)
			continue;
		if (!WindowUnderRoot(w, root))
			continue;
		paintWindow(w);
	}
}
