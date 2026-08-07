#include "Gw2Ui.h"
#include "Gw2UiInternal.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "nexus/Nexus.h"

#include <algorithm>

void Gw2Ui::PaintNativeScrollbars(float opacity, ImGuiWindow* root)
{
	if (!root)
		root = ImGui::GetCurrentWindow();
	if (!root || root->Collapsed)
		return;

	float a = opacity;
	if (a < 0.f) a = 0.f;
	if (a > 1.f) a = 1.f;
	const ImU32 tint = ImGui::ColorConvertFloat4ToU32(ImVec4(1.f, 1.f, 1.f, a));
	const ImU32 cover = ImGui::ColorConvertFloat4ToU32(ImVec4(0.04f, 0.03f, 0.02f, 0.98f * a));
	const ImU32 track = ImGui::ColorConvertFloat4ToU32(ImVec4(0.05f, 0.04f, 0.03f, 0.95f * a));

	Texture_t* thumb = Gw2UiDetail::GetChromeNamed("scroll-thumb");
	Texture_t* mid = Gw2UiDetail::GetChromeNamed("scroll-thumb-mid");
	Texture_t* cap = Gw2UiDetail::GetChromeNamed("scroll-thumb-cap");
	Texture_t* arrow = Gw2UiDetail::GetChromeNamed("scroll-arrow");
	if ((!thumb || !thumb->Resource) && (!mid || !mid->Resource))
		return;

	auto paintWindow = [&](ImGuiWindow* window, auto&& self) -> void {
		if (!window || window->Collapsed)
			return;
		ImDrawList* dl = window->DrawList;
		if (!dl)
			return;

		auto drawAxis = [&](ImGuiAxis axis) {
			if (axis == ImGuiAxis_Y && !window->ScrollbarY)
				return;
			if (axis == ImGuiAxis_X && !window->ScrollbarX)
				return;

			const ImRect bb = ImGui::GetWindowScrollbarRect(window, axis);
			if (bb.GetWidth() < 2.f || bb.GetHeight() < 2.f)
				return;

			/* After End(), DrawList clip is content-only and excludes the gutter —
			   replace clip so DAT chrome is not discarded. */
			dl->PushClipRect(bb.Min, bb.Max, /*intersect_with_current=*/false);

			/* Obliterate ImGui's gold grab / track under the native art. */
			dl->AddRectFilled(bb.Min, bb.Max, cover);

			const float size = (axis == ImGuiAxis_Y) ? bb.GetWidth() : bb.GetHeight();
			const float trackLen = (axis == ImGuiAxis_Y) ? bb.GetHeight() : bb.GetWidth();
			const float arrowH = size;
			const float scroll = (axis == ImGuiAxis_Y) ? window->Scroll.y : window->Scroll.x;
			const float scrollMax = (axis == ImGuiAxis_Y) ? window->ScrollMax.y : window->ScrollMax.x;
			const float winSize = (axis == ImGuiAxis_Y) ? window->InnerRect.GetHeight() : window->InnerRect.GetWidth();
			const float content = winSize + scrollMax;
			float grabLen = (content > 1.f) ? (winSize / content) * (trackLen - arrowH * 2.f) : trackLen;
			if (grabLen < size * 1.6f)
				grabLen = size * 1.6f;
			const float travel = (std::max)(0.f, trackLen - arrowH * 2.f - grabLen);
			const float t = (scrollMax > 0.f) ? (scroll / scrollMax) : 0.f;
			const float grabOff = arrowH + travel * t;

			if (axis == ImGuiAxis_Y)
			{
				const float cx = (bb.Min.x + bb.Max.x) * 0.5f;
				dl->AddLine(ImVec2(cx, bb.Min.y + arrowH), ImVec2(cx, bb.Max.y - arrowH), track, 1.5f);
			}
			else
			{
				const float cy = (bb.Min.y + bb.Max.y) * 0.5f;
				dl->AddLine(ImVec2(bb.Min.x + arrowH, cy), ImVec2(bb.Max.x - arrowH, cy), track, 1.5f);
			}

			auto blit = [&](Texture_t* tex, ImVec2 p0, ImVec2 p1, bool flipV) {
				if (!tex || !tex->Resource)
					return;
				ImVec2 uv0(0.f, flipV ? 1.f : 0.f);
				ImVec2 uv1(1.f, flipV ? 0.f : 1.f);
				dl->AddImage(reinterpret_cast<ImTextureID>(tex->Resource), p0, p1, uv0, uv1, tint);
			};

			if (axis == ImGuiAxis_Y)
			{
				if (arrow && arrow->Resource)
				{
					blit(arrow, ImVec2(bb.Min.x, bb.Min.y), ImVec2(bb.Max.x, bb.Min.y + arrowH), true);
					blit(arrow, ImVec2(bb.Min.x, bb.Max.y - arrowH), ImVec2(bb.Max.x, bb.Max.y), false);
				}

				const float gy0 = bb.Min.y + grabOff;
				const float gy1 = gy0 + grabLen;
				const float capH = size * 0.55f;
				Texture_t* body = (thumb && thumb->Resource) ? thumb : mid;
				if (cap && cap->Resource && grabLen > capH * 2.5f)
				{
					blit(cap, ImVec2(bb.Min.x, gy0), ImVec2(bb.Max.x, gy0 + capH), false);
					blit(body, ImVec2(bb.Min.x, gy0 + capH * 0.85f),
						ImVec2(bb.Max.x, gy1 - capH * 0.85f), false);
					blit(cap, ImVec2(bb.Min.x, gy1 - capH), ImVec2(bb.Max.x, gy1), true);
				}
				else if (body && body->Resource)
				{
					blit(body, ImVec2(bb.Min.x, gy0), ImVec2(bb.Max.x, gy1), false);
				}
			}
			else
			{
				Texture_t* body = (mid && mid->Resource) ? mid : thumb;
				if (body && body->Resource)
				{
					const float gx0 = bb.Min.x + grabOff;
					blit(body, ImVec2(gx0, bb.Min.y), ImVec2(gx0 + grabLen, bb.Max.y), false);
				}
			}

			dl->PopClipRect();
		};

		drawAxis(ImGuiAxis_Y);
		drawAxis(ImGuiAxis_X);
		for (ImGuiWindow* child : window->DC.ChildWindows)
			self(child, self);
	};

	paintWindow(root, paintWindow);
}
