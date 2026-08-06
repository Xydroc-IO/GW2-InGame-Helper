#include "Gw2Ui.h"
#include "Gw2UiInternal.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "UiChrome.h"

#include "imgui/imgui.h"
#include "nexus/Nexus.h"

#include <cfloat>
#include <cstring>

namespace
{
	Texture_t* GetChromeTex(int assetId)
	{
		if (assetId <= 0 || !G::API || !G::API->Textures_Get)
			return nullptr;
		char id[48];
		UiChrome::MakeTexId(assetId, id, sizeof(id));
		Texture_t* tex = G::API->Textures_Get(id);
		if (!tex || !tex->Resource)
		{
			/* Lazy warm if load raced ahead of extract. */
			UiChrome::WarmTextures(AddonPaths::DataDir());
			tex = G::API->Textures_Get(id);
		}
		if (tex && tex->Resource)
			return tex;
		/* CDN fallback (same IDs Tyrian Codex / Blish use) if pack missing. */
		Gw2Ui::Request(assetId);
		return Gw2UiDetail::GetTex(assetId);
	}

	Texture_t* GetChromeNamed(const char* stem)
	{
		if (!stem || !stem[0] || !G::API || !G::API->Textures_Get)
			return nullptr;
		char id[80];
		UiChrome::MakeNamedTexId(stem, id, sizeof(id));
		Texture_t* tex = G::API->Textures_Get(id);
		if (!tex || !tex->Resource)
		{
			UiChrome::WarmTextures(AddonPaths::DataDir());
			tex = G::API->Textures_Get(id);
		}
		if (!tex || !tex->Resource)
			return nullptr;
		return tex;
	}
}

bool Gw2Ui::PaintPadChrome(float opacity)
{
	/*
	 * Full-bleed wash plate (no black matte inset). Soft ink fringe only on
	 * L/R/bottom so the rim feathers into the game like Hero — not a thick frame.
	 * Top stays flush for the title bar.
	 */
	ImDrawList* dl = ImGui::GetWindowDrawList();
	if (!dl)
		return false;

	float a = opacity;
	if (a < 0.f)
		a = 0.f;
	if (a > 1.f)
		a = 1.f;

	const ImVec2 p0 = ImGui::GetWindowPos();
	const ImVec2 sz = ImGui::GetWindowSize();
	const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);

	Texture_t* wash = GetChromeNamed("panel-wash");
	Texture_t* fill = (wash && wash->Resource) ? wash : GetChromeTex(static_cast<int>(Icon::PanelFill));
	if (!fill || !fill->Resource)
		fill = GetChromeTex(static_cast<int>(Icon::PanelFillAlt));
	const bool usingWash = (wash && wash->Resource && fill == wash);

	dl->PushClipRect(p0, p1, false);
	dl->AddRectFilled(p0, p1, IM_COL32(10, 8, 6, static_cast<int>(a * 255.f + 0.5f)));

	if (!fill || !fill->Resource)
	{
		dl->AddRect(p0, p1, IM_COL32(161, 120, 56, static_cast<int>(a * 200.f + 0.5f)));
		dl->PopClipRect();
		return false;
	}

	const ImU32 col = IM_COL32(255, 255, 255, static_cast<int>(a * 255.f + 0.5f));
	ImVec2 uv0(0.f, 0.f), uv1(1.f, 1.f);
	if (!usingWash)
	{
		constexpr float tex = 1024.f;
		uv0 = ImVec2(48.f / tex, 34.f / tex);
		uv1 = ImVec2((40.f + 905.f) / tex, (26.f + 680.f) / tex);
	}
	dl->AddImage(reinterpret_cast<ImTextureID>(fill->Resource),
		p0, p1, uv0, uv1, col);
	dl->AddRectFilled(p0, p1, IM_COL32(6, 4, 3, static_cast<int>(a * 55.f + 0.5f)));
	dl->PopClipRect();

	/* Soft brush fringe only — never a solid black mat around the window. */
	Texture_t* ink = GetChromeNamed("ink-edge");
	if (!ink || !ink->Resource)
		ink = GetChromeTex(static_cast<int>(Icon::InkEdge));
	if (ink && ink->Resource)
	{
		constexpr float kFringe = 18.f;
		constexpr float kBleed = 10.f;
		ImDrawList* edgeDl = ImGui::GetForegroundDrawList();
		const ImTextureID iid = reinterpret_cast<ImTextureID>(ink->Resource);
		const ImU32 inkCol = IM_COL32(255, 255, 255, static_cast<int>(a * 180.f + 0.5f));

		edgeDl->PushClipRect(
			ImVec2(p0.x - kBleed - 2.f, p0.y),
			ImVec2(p1.x + kBleed + 2.f, p1.y + kBleed + 2.f),
			false);

		edgeDl->AddImage(iid,
			ImVec2(p0.x - 2.f, p1.y - kFringe),
			ImVec2(p1.x + 2.f, p1.y + kBleed),
			ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), inkCol);

		edgeDl->AddImageQuad(iid,
			ImVec2(p0.x - kBleed, p0.y),
			ImVec2(p0.x + kFringe, p0.y),
			ImVec2(p0.x + kFringe, p1.y + kBleed),
			ImVec2(p0.x - kBleed, p1.y + kBleed),
			ImVec2(0.f, 1.f), ImVec2(0.f, 0.f),
			ImVec2(1.f, 0.f), ImVec2(1.f, 1.f),
			inkCol);

		edgeDl->AddImageQuad(iid,
			ImVec2(p1.x - kFringe, p0.y),
			ImVec2(p1.x + kBleed, p0.y),
			ImVec2(p1.x + kBleed, p1.y + kBleed),
			ImVec2(p1.x - kFringe, p1.y + kBleed),
			ImVec2(0.f, 0.f), ImVec2(0.f, 1.f),
			ImVec2(1.f, 1.f), ImVec2(1.f, 0.f),
			inkCol);

		edgeDl->PopClipRect();
	}

	return true;
}

bool Gw2Ui::DrawPadTitleBar(const char* title, bool* pOpen, float opacity)
{
	float a = opacity;
	if (a < 0.f)
		a = 0.f;
	if (a > 1.f)
		a = 1.f;

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

	/* Expanded title — 156046 strip flush to window top; larger crest + − / X. */
	const float kTitleH = collapsed ? 28.f : 60.f;
	const float kEmblem = collapsed ? 0.f : 104.f;
	const float kEmblemHangTop = collapsed ? 0.f : 26.f;
	const float kBtn = collapsed ? 22.f : 38.f;
	const float kExitSz = collapsed ? 20.f : 34.f;
	constexpr float kPadX = 12.f;
	constexpr float kBtnGap = 8.f;
	constexpr float kTitleGap = 8.f;
	/* Keep X near the frame edge (theme WindowPadding is for body chrome only). */
	constexpr float kChromeInsetR = 4.f;

	char vis[96];
	Gw2UiDetail::VisibleLabel(title, vis, sizeof(vis));
	if (char* hash = std::strstr(vis, "##"))
		*hash = '\0';

	const ImVec2 win0 = ImGui::GetWindowPos();
	const float winW = ImGui::GetWindowWidth();
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
	/* Leave room for the overhanging crest so the drag strip doesn't start under it. */
	const float dragLeft = (!collapsed && kEmblem > 0.f)
		? (kEmblem * 0.55f + kPadX) : 0.f;
	const float dragW = winW - dragLeft - btnsW - rightInset;
	const float dragWClamped = dragW > 24.f ? dragW : 24.f;
	if (dragLeft > 0.f)
		ImGui::SetCursorScreenPos(ImVec2(win0.x + dragLeft, row0.y));

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImU32 col = IM_COL32(255, 255, 255, static_cast<int>(a * 255.f + 0.5f));
	/* Controls stay warm gold; title text matches in-game Hero cream. */
	const ImU32 gold = IM_COL32(255, 236, 170, static_cast<int>(a * 255.f + 0.5f));
	const ImU32 titleCol = IM_COL32(255, 250, 235, static_cast<int>(a * 255.f + 0.5f));
	const ImU32 titleShadow = IM_COL32(0, 0, 0, static_cast<int>(a * 210.f + 0.5f));

	if (dl)
	{
		const ImVec2 t0 = win0;
		const ImVec2 t1(win0.x + winW, win0.y + kTitleH + (collapsed ? 4.f : 0.f));
		dl->PushClipRect(t0, ImVec2(t1.x, t1.y + 2.f), false);

		/* Opaque 156046 strip — full width, flush with the window top edge. */
		Texture_t* titleBar = GetChromeNamed("title-bar");
		/* Solid underpaint so feathered pack fringes never leave a tan rim at the top. */
		dl->AddRectFilled(t0, t1, IM_COL32(14, 11, 8, static_cast<int>(a * 255.f + 0.5f)));
		if (titleBar && titleBar->Resource && !collapsed)
		{
			dl->AddImage(reinterpret_cast<ImTextureID>(titleBar->Resource),
				t0, t1, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), col);
		}
		else if (!collapsed)
		{
			Texture_t* fill = GetChromeTex(static_cast<int>(Icon::PanelFill));
			if (fill && fill->Resource)
			{
				constexpr float tex = 1024.f;
				const float u0 = 40.f / tex;
				const float u1 = (40.f + 913.f) / tex;
				const float v0 = 26.f / tex;
				const float v1 = (26.f + 78.f) / tex;
				dl->AddImage(reinterpret_cast<ImTextureID>(fill->Resource),
					t0, t1, ImVec2(u0, v0), ImVec2(u1, v1), col);
			}
			dl->AddRectFilled(t0, t1,
				IM_COL32(8, 7, 6, static_cast<int>(a * 165.f + 0.5f)));
		}
		if (collapsed)
			dl->AddRectFilled(t0, t1, IM_COL32(8, 7, 6, static_cast<int>(a * 235.f + 0.5f)));
		/* Under-edge of the title strip (meets body below). */
		dl->AddLine(
			ImVec2(t0.x + 1.f, t1.y - 1.f),
			ImVec2(t1.x - 1.f, t1.y - 1.f),
			IM_COL32(55, 48, 38, static_cast<int>(a * 160.f + 0.5f)), 1.0f);
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
		float textX = row0.x + kPadX;
		if (!collapsed && kEmblem > 0.f)
		{
			/* Pack crest (128² upscale of Hero 157085) — CDN icon is only 32² and blurs. */
			Texture_t* emblem = GetChromeNamed("crest-hero");
			if (!emblem || !emblem->Resource)
			{
				Gw2Ui::Request(Icon::Hero);
				emblem = Gw2UiDetail::GetTex(static_cast<int>(Icon::Hero));
			}
			if (!emblem || !emblem->Resource)
				emblem = GetChromeTex(static_cast<int>(Icon::WindowEmblem));
			const float ex = win0.x - 6.f; /* hang past the left frame like Hero */
			const float ey = win0.y - kEmblemHangTop;
			dl->PushClipRect(
				ImVec2(win0.x - 12.f, win0.y - kEmblemHangTop - 4.f),
				ImVec2(win0.x + kEmblem + 12.f, win0.y + kEmblem - kEmblemHangTop + 4.f),
				false);
			if (emblem && emblem->Resource)
			{
				dl->AddImage(reinterpret_cast<ImTextureID>(emblem->Resource),
					ImVec2(ex, ey),
					ImVec2(ex + kEmblem, ey + kEmblem),
					ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), col);
				textX = ex + kEmblem + kTitleGap;
			}
			dl->PopClipRect();
		}

		/* Cream title vertically centered in the dark plate (not the crest). */
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
			Texture_t* cxTex = GetChromeNamed(closeHover ? "button-exit-active" : "button-exit");
			if (!cxTex || !cxTex->Resource)
				cxTex = GetChromeNamed("button-exit");
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
		const ImVec2 sz = ImGui::GetWindowSize();
		ImGui::SetWindowSize(ImVec2(sz.x, kTitleH + 4.f));
		ImGui::SetCursorScreenPos(ImVec2(row0.x, row0.y + kTitleH));
		if (slimPad)
			ImGui::PopStyleVar(); /* WindowPadding */
		return false;
	}

	if (slimPad)
		ImGui::PopStyleVar(); /* WindowPadding — restored mid-frame */

	/* Apply saved pre-minimize size once on restore. */
	if (st->GetBool(restoreId, false))
	{
		const float rw = st->GetFloat(expandedWId, 0.f);
		const float rh = st->GetFloat(expandedHId, 0.f);
		const ImVec2 sz = ImGui::GetWindowSize();
		const float useW = rw >= 80.f ? rw : sz.x;
		const float useH = rh >= 80.f ? rh : 400.f;
		ImGui::SetWindowSize(ImVec2(useW, useH));
		st->SetBool(restoreId, false);
	}

	ImGui::SetCursorScreenPos(ImVec2(win0.x + bodyPadX, win0.y + kTitleH + 2.f));
	return true;
}
