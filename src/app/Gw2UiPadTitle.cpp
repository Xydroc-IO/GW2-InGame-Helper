#include "Gw2Ui.h"
#include "Gw2UiInternal.h"

#include "Globals.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "nexus/Nexus.h"

#include <cfloat>
#include <cstring>

bool Gw2Ui::DrawPadTitleBar(const char* title, bool* pOpen, float opacity, float leftExtend)
{
	float a = opacity;
	if (a < 0.f)
		a = 0.f;
	if (a > 1.f)
		a = 1.f;
	if (leftExtend < 0.f)
		leftExtend = 0.f;

	ImGuiStorage* st = ImGui::GetStateStorage();
	const ImGuiID collapsedId = ImGui::GetID("##gw2igh_pad_collapsed");
	const ImGuiID expandedWId = ImGui::GetID("##gw2igh_pad_expanded_w");
	const ImGuiID expandedHId = ImGui::GetID("##gw2igh_pad_expanded_h");
	const ImGuiID restoreId = ImGui::GetID("##gw2igh_pad_restore");
	bool collapsed = st->GetBool(collapsedId, false);

	/* Keep last expanded size while open so restore matches the user's window. */
	{
		const ImVec2 live = ImGui::GetWindowSize();
		if (!collapsed && live.y >= 80.f && live.x >= 80.f)
		{
			st->SetFloat(expandedWId, live.x);
			st->SetFloat(expandedHId, live.y);
		}
	}

	auto toggleCollapsed = [&]() {
		if (!collapsed)
		{
			const ImVec2 live = ImGui::GetWindowSize();
			if (live.y >= 80.f && live.x >= 80.f)
			{
				st->SetFloat(expandedWId, live.x);
				st->SetFloat(expandedHId, live.y);
			}
			collapsed = true;
			st->SetBool(collapsedId, true);
			st->SetBool(restoreId, false);
		}
		else
		{
			collapsed = false;
			st->SetBool(collapsedId, false);
			st->SetBool(restoreId, true);
		}
	};

	/* Expanded title — 156046 strip flush to window top. */
	const float kTitleH = collapsed ? 28.f : 60.f;
	const float kBtn = collapsed ? 22.f : 38.f;
	const float kExitSz = collapsed ? 20.f : 34.f;
	constexpr float kPadX = 12.f;
	constexpr float kBtnGap = 8.f;
	/* Keep X near the frame edge (theme WindowPadding is for body chrome only). */
	constexpr float kChromeInsetR = 4.f;

	char vis[96];
	Gw2UiDetail::VisibleLabel(title, vis, sizeof(vis));
	if (char* hash = std::strstr(vis, "##"))
		*hash = '\0';

	const ImVec2 win0 = ImGui::GetWindowPos();
	const float winW = ImGui::GetWindowWidth();
	/* Strip may overhang left (side rail); right edge stays at window right. */
	const ImVec2 title0(win0.x - leftExtend, win0.y);
	const ImGuiStyle& style = ImGui::GetStyle();
	const float bodyPadX = style.WindowPadding.x;
	const float contentMaxX = ImGui::GetWindowContentRegionMax().x;
	/* Title controls sit near the right edge — do not inherit body WindowPadding. */
	float rightInset = kChromeInsetR;
	if (contentMaxX > 80.f && winW - contentMaxX > rightInset + 2.f)
		rightInset = (winW - contentMaxX) * 0.35f + kChromeInsetR;
	if (rightInset < kChromeInsetR)
		rightInset = kChromeInsetR;

	/*
	 * Title connects to the window TOP — ignore WindowPadding for the title row.
	 * Body padding applies only below the strip (Hero: bar IS the top of the panel).
	 */
	ImGui::SetCursorScreenPos(win0);
	const ImVec2 row0 = win0;

	/* Kill padding on the minimized strip so height stays ~one title line. */
	const bool slimPad = collapsed;
	if (slimPad)
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.f, 2.f));

	const int nBtns = pOpen ? 2 : 1;
	const float btnsW = static_cast<float>(nBtns) * kBtn + static_cast<float>(nBtns - 1) * kBtnGap + kPadX;
	const float dragW = winW - btnsW - rightInset;
	const float dragWClamped = dragW > 24.f ? dragW : 24.f;

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImU32 col = IM_COL32(255, 255, 255, static_cast<int>(a * 255.f + 0.5f));
	/* Controls stay warm gold; title text matches in-game Hero cream. */
	const ImU32 gold = IM_COL32(255, 236, 170, static_cast<int>(a * 255.f + 0.5f));
	const ImU32 titleCol = IM_COL32(255, 250, 235, static_cast<int>(a * 255.f + 0.5f));
	const ImU32 titleShadow = IM_COL32(0, 0, 0, static_cast<int>(a * 210.f + 0.5f));

	if (dl)
	{
		const ImVec2 t0 = title0;
		const ImVec2 t1(win0.x + winW, win0.y + kTitleH + (collapsed ? 4.f : 0.f));
		dl->PushClipRect(t0, ImVec2(t1.x, t1.y + 2.f), false);

		/*
		 * Hero title fade: dense left, soft brush to the game on the right.
		 * GPU-interpolated vertex alpha (not banded slices) — body wash starts
		 * below this strip so the fade reveals the world.
		 */
		Texture_t* titleBar = Gw2UiDetail::GetChromeNamed("title-bar");
		Texture_t* fillFallback = nullptr;
		if ((!titleBar || !titleBar->Resource) && !collapsed)
			fillFallback = Gw2UiDetail::GetChromeTex(static_cast<int>(Icon::PanelFill));

		auto imageHFade = [&](ImTextureID tex, ImVec2 pmin, ImVec2 pmax,
			ImVec2 uvmin, ImVec2 uvmax, ImU32 colL, ImU32 colR) {
			if (((colL | colR) & IM_COL32_A_MASK) == 0)
				return;
			const bool push = tex != dl->_CmdHeader.TextureId;
			if (push)
				dl->PushTextureID(tex);
			dl->PrimReserve(6, 4);
			dl->PrimQuadUV(
				ImVec2(pmin.x, pmin.y), ImVec2(pmax.x, pmin.y),
				ImVec2(pmax.x, pmax.y), ImVec2(pmin.x, pmax.y),
				uvmin, ImVec2(uvmax.x, uvmin.y),
				uvmax, ImVec2(uvmin.x, uvmax.y),
				colL);
			/* PrimQuadUV wrote one color — rewrite right-edge verts to colR. */
			ImDrawVert* v = dl->VtxBuffer.Data + (dl->VtxBuffer.Size - 4);
			v[1].col = colR; /* TR */
			v[2].col = colR; /* BR */
			if (push)
				dl->PopTextureID();
		};

		if (!collapsed)
		{
			const float stripW = t1.x - t0.x;
			/*
			 * Hero: dense left → soft mid (world shows) → lil grey pocket on the
			 * far right behind − / X (never fade all the way to clear).
			 */
			constexpr float kHold = 0.28f;
			constexpr float kTrough = 0.76f;
			const float xHold = t0.x + stripW * kHold;
			const float xTrough = t0.x + stripW * kTrough;

			const int aSolid = static_cast<int>(a * 255.f + 0.5f);
			const int aUnder = static_cast<int>(a * 210.f + 0.5f);
			const int aTroughTex = static_cast<int>(a * 40.f + 0.5f);
			const int aCornerTex = static_cast<int>(a * 165.f + 0.5f);
			const int aTroughUnd = static_cast<int>(a * 32.f + 0.5f);
			const int aCornerUnd = static_cast<int>(a * 150.f + 0.5f);
			const ImU32 texSolid = IM_COL32(255, 255, 255, aSolid);
			const ImU32 texTrough = IM_COL32(255, 255, 255, aTroughTex);
			const ImU32 texCorner = IM_COL32(255, 255, 255, aCornerTex);
			const ImU32 undSolid = IM_COL32(14, 11, 8, aUnder);
			const ImU32 undTrough = IM_COL32(14, 11, 8, aTroughUnd);
			const ImU32 undCorner = IM_COL32(14, 11, 8, aCornerUnd);

			dl->AddRectFilled(t0, ImVec2(xHold, t1.y), undSolid);
			dl->AddRectFilledMultiColor(
				ImVec2(xHold, t0.y), ImVec2(xTrough, t1.y),
				undSolid, undTrough, undTrough, undSolid);
			dl->AddRectFilledMultiColor(
				ImVec2(xTrough, t0.y), t1,
				undTrough, undCorner, undCorner, undTrough);

			auto drawSeg = [&](float x0, float x1, float u0, float u1, ImU32 cL, ImU32 cR) {
				const ImVec2 pmin(x0, t0.y);
				const ImVec2 pmax(x1, t1.y);
				if (titleBar && titleBar->Resource)
				{
					imageHFade(reinterpret_cast<ImTextureID>(titleBar->Resource),
						pmin, pmax, ImVec2(u0, 0.f), ImVec2(u1, 1.f), cL, cR);
				}
				else if (fillFallback && fillFallback->Resource)
				{
					constexpr float tex = 1024.f;
					imageHFade(reinterpret_cast<ImTextureID>(fillFallback->Resource),
						pmin, pmax,
						ImVec2((40.f + 913.f * u0) / tex, 26.f / tex),
						ImVec2((40.f + 913.f * u1) / tex, (26.f + 78.f) / tex),
						cL, cR);
				}
			};
			drawSeg(t0.x, xHold, 0.f, kHold, texSolid, texSolid);
			drawSeg(xHold, xTrough, kHold, kTrough, texSolid, texTrough);
			drawSeg(xTrough, t1.x, kTrough, 1.f, texTrough, texCorner);

			/* Hairline under-edge follows the same hold → trough → corner pocket. */
			dl->AddRectFilled(
				ImVec2(t0.x, t1.y - 1.5f), ImVec2(xHold, t1.y - 0.5f),
				IM_COL32(55, 48, 38, static_cast<int>(a * 160.f + 0.5f)));
			dl->AddRectFilledMultiColor(
				ImVec2(xHold, t1.y - 1.5f), ImVec2(xTrough, t1.y - 0.5f),
				IM_COL32(55, 48, 38, static_cast<int>(a * 160.f + 0.5f)),
				IM_COL32(55, 48, 38, static_cast<int>(a * 40.f + 0.5f)),
				IM_COL32(55, 48, 38, static_cast<int>(a * 40.f + 0.5f)),
				IM_COL32(55, 48, 38, static_cast<int>(a * 160.f + 0.5f)));
			dl->AddRectFilledMultiColor(
				ImVec2(xTrough, t1.y - 1.5f), ImVec2(t1.x, t1.y - 0.5f),
				IM_COL32(55, 48, 38, static_cast<int>(a * 40.f + 0.5f)),
				IM_COL32(55, 48, 38, static_cast<int>(a * 120.f + 0.5f)),
				IM_COL32(55, 48, 38, static_cast<int>(a * 120.f + 0.5f)),
				IM_COL32(55, 48, 38, static_cast<int>(a * 40.f + 0.5f)));
		}
		else
			dl->AddRectFilled(t0, t1, IM_COL32(8, 7, 6, static_cast<int>(a * 235.f + 0.5f)));

		dl->PopClipRect();
	}

	ImGui::InvisibleButton("##gw2igh_pad_title_drag", ImVec2(dragWClamped, kTitleH));
	const bool dragHovered = ImGui::IsItemHovered();
	if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
	{
		const ImVec2 d = ImGui::GetIO().MouseDelta;
		ImGui::SetWindowPos(ImVec2(win0.x + d.x, win0.y + d.y));
	}
	if (dragHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		toggleCollapsed();

	if (dl)
	{
		/* Title starts on the helper body; crest sits centered over the side rail. */
		float textX = win0.x + kPadX;

		/*
		 * Amber gem crest: centered on the left nav column (title strip overhang),
		 * hangs above the strip like Hero. Title text sits to its right on the body.
		 */
		if (!collapsed && leftExtend > 0.f)
		{
			Texture_t* crest = Gw2UiDetail::GetChromeNamed("crest-hero");
			if (crest && crest->Resource)
			{
				constexpr float kHangTop = 22.f;
				/* Clear of the gem glow — keep letters fully readable. */
				constexpr float kCrestGap = 14.f;
				float kCrest = 88.f;
				/* Keep crest roughly within the rail width (+ overhang for Hero hang). */
				if (kCrest > leftExtend + 40.f)
					kCrest = leftExtend + 40.f;
				if (kCrest < 48.f)
					kCrest = 48.f;
				const float railMid = title0.x + leftExtend * 0.5f;
				const float cx0 = railMid - kCrest * 0.5f;
				const float cy0 = win0.y - kHangTop;
				dl->PushClipRect(
					ImVec2(cx0 - 4.f, cy0 - 2.f),
					ImVec2(cx0 + kCrest + 4.f, win0.y + kTitleH + 10.f),
					false);
				dl->AddImage(reinterpret_cast<ImTextureID>(crest->Resource),
					ImVec2(cx0, cy0), ImVec2(cx0 + kCrest, cy0 + kCrest),
					ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), col);
				dl->PopClipRect();
				textX = cx0 + kCrest + kCrestGap;
			}
		}

		/* Cream title vertically centered in the dark plate. */
		ImFont* font = ImGui::GetFont();
		const float baseSz = ImGui::GetFontSize();
		const float titleSz = collapsed ? baseSz : baseSz * 1.65f;
		const ImVec2 tsz = font
			? font->CalcTextSizeA(titleSz, FLT_MAX, 0.f, vis)
			: ImGui::CalcTextSize(vis);
		const float ty = win0.y + (kTitleH - tsz.y) * 0.5f;
		const ImVec2 tp(textX, ty);
		if (font)
		{
			static const ImVec2 kHalo[] = {
				{ -1.5f, 0.f }, { 1.5f, 0.f }, { 0.f, -1.5f }, { 0.f, 1.5f },
				{ -1.2f, -1.2f }, { 1.2f, -1.2f }, { -1.2f, 1.2f }, { 1.2f, 1.2f },
				{ 0.f, 2.2f },
			};
			for (const ImVec2& o : kHalo)
				dl->AddText(font, titleSz, ImVec2(tp.x + o.x, tp.y + o.y), titleShadow, vis);
			dl->AddText(font, titleSz, tp, titleCol, vis);
		}
		else
		{
			dl->AddText(ImVec2(tp.x + 1.5f, tp.y + 1.5f), titleShadow, vis);
			dl->AddText(tp, titleCol, vis);
		}
	}

	/* Controls sit in the top-right of the plate like native panel chrome. */
	ImGui::SetCursorScreenPos(ImVec2(
		win0.x + winW - btnsW - rightInset,
		win0.y + (kTitleH - kBtn) * 0.5f));
	if (dl)
		dl->PushClipRect(
			ImVec2(win0.x, win0.y - 2.f),
			ImVec2(win0.x + winW, win0.y + kTitleH + 8.f),
			false);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.10f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.18f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);

	if (ImGui::Button("##gw2igh_pad_min", ImVec2(kBtn, kBtn)))
		toggleCollapsed();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(collapsed ? "Restore" : "Minimize");
	if (dl)
	{
		const ImVec2 mn = ImGui::GetItemRectMin();
		const ImVec2 mx = ImGui::GetItemRectMax();
		const float cx = (mn.x + mx.x) * 0.5f;
		const float cy = (mn.y + mx.y) * 0.5f;
		const float hw = collapsed ? 5.5f : 8.f;
		if (collapsed)
			dl->AddRect(ImVec2(cx - hw, cy - hw), ImVec2(cx + hw, cy + hw), gold, 0.f, 0, 1.8f);
		else
			dl->AddLine(ImVec2(cx - hw, cy), ImVec2(cx + hw, cy), gold, 2.8f);
	}

	if (pOpen)
	{
		ImGui::SameLine(0.f, kBtnGap);
		if (ImGui::Button("##gw2igh_pad_close", ImVec2(kBtn, kBtn)))
			*pOpen = false;
		const bool closeHover = ImGui::IsItemHovered();
		if (closeHover)
			ImGui::SetTooltip("Close");
		const ImVec2 mn = ImGui::GetItemRectMin();
		const ImVec2 mx = ImGui::GetItemRectMax();
		if (dl)
		{
			/* Contacts / WindowBase2 exit — draw glyph smaller than the hit box. */
			Texture_t* cxTex = Gw2UiDetail::GetChromeNamed(closeHover ? "button-exit-active" : "button-exit");
			if (!cxTex || !cxTex->Resource)
				cxTex = Gw2UiDetail::GetChromeNamed("button-exit");
			if (cxTex && cxTex->Resource)
			{
				const float s = kExitSz;
				const float ix = (mn.x + mx.x - s) * 0.5f;
				const float iy = (mn.y + mx.y - s) * 0.5f;
				dl->AddImage(reinterpret_cast<ImTextureID>(cxTex->Resource),
					ImVec2(ix, iy), ImVec2(ix + s, iy + s),
					ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), col);
			}
			else
			{
				const float cx = (mn.x + mx.x) * 0.5f;
				const float cy = (mn.y + mx.y) * 0.5f;
				const float hw = 7.5f;
				dl->AddLine(ImVec2(cx - hw, cy - hw), ImVec2(cx + hw, cy + hw), gold, 2.4f);
				dl->AddLine(ImVec2(cx + hw, cy - hw), ImVec2(cx - hw, cy + hw), gold, 2.4f);
			}
		}
	}

	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(3);
	if (dl)
		dl->PopClipRect();

	if (collapsed)
	{
		/* Size constraints (PadDock / helper minH) otherwise leave a tall invisible hitbox. */
		const float barH = kTitleH + 4.f;
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window)
		{
			window->Flags |= ImGuiWindowFlags_NoResize;
			window->Size.y = barH;
			window->SizeFull.y = barH;
			window->ContentSize.y = 0.f;
			window->ContentSizeIdeal.y = 0.f;
		}
		ImGui::SetWindowSize(ImVec2(ImGui::GetWindowSize().x, barH));
		ImGui::SetCursorScreenPos(ImVec2(row0.x, row0.y + kTitleH));
		if (slimPad)
			ImGui::PopStyleVar(); /* WindowPadding */
		return false;
	}

	if (slimPad)
		ImGui::PopStyleVar(); /* WindowPadding — restored mid-frame */

	/* Apply saved pre-minimize size (this frame + one hold frame). */
	{
		const ImGuiID restoreHoldId = ImGui::GetID("##gw2igh_pad_restore_hold");
		const bool doRestore = st->GetBool(restoreId, false) || st->GetBool(restoreHoldId, false);
		if (doRestore)
		{
			const float rw = st->GetFloat(expandedWId, 0.f);
			const float rh = st->GetFloat(expandedHId, 0.f);
			const ImVec2 sz = ImGui::GetWindowSize();
			const float useW = rw >= 80.f ? rw : (sz.x >= 80.f ? sz.x : 560.f);
			const float useH = rh >= 80.f ? rh : 400.f;
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (window)
			{
				window->Flags &= ~ImGuiWindowFlags_NoResize;
				window->Size = ImVec2(useW, useH);
				window->SizeFull = ImVec2(useW, useH);
			}
			ImGui::SetWindowSize(ImVec2(useW, useH));
			if (st->GetBool(restoreId, false))
			{
				st->SetBool(restoreId, false);
				st->SetBool(restoreHoldId, true); /* re-apply next frame after constraints */
			}
			else
				st->SetBool(restoreHoldId, false);

			/* Main helper: keep persisted geom in sync with restore. */
			if (title && std::strstr(title, "Game Helper"))
			{
				G::WindowWidth = useW;
				G::WindowHeight = useH;
				G::HasSavedSize = true;
			}
		}
	}

	ImGui::SetCursorScreenPos(ImVec2(win0.x + bodyPadX, win0.y + kTitleH + 6.f));
	return true;
}
