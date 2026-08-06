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
	 * Blish StandardWindow (controls/window/155985):
	 *   windowRegion  = (40, 26, 913, 691) on 1024²
	 * Drawn on the window list with NoBackground so the texture IS the frame
	 * (not wallpaper under stock ImGui title chrome).
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

	Texture_t* fill = GetChromeTex(static_cast<int>(Icon::PanelFill));
	if (!fill || !fill->Resource)
		fill = GetChromeTex(static_cast<int>(Icon::PanelFillAlt));

	dl->PushClipRect(p0, p1, false);

	if (!fill || !fill->Resource)
	{
		const ImU32 bg = IM_COL32(14, 11, 8, static_cast<int>(a * 245.f + 0.5f));
		dl->AddRectFilled(p0, p1, bg);
		dl->AddRect(p0, p1, IM_COL32(161, 120, 56, static_cast<int>(a * 200.f + 0.5f)));
		dl->PopClipRect();
		return false;
	}

	constexpr float tex = 1024.f;
	constexpr float u0 = 40.f / tex;
	constexpr float v0 = 26.f / tex;
	constexpr float u1 = (40.f + 913.f) / tex;
	constexpr float v1 = (26.f + 691.f) / tex;
	const ImU32 col = IM_COL32(255, 255, 255, static_cast<int>(a * 255.f + 0.5f));
	dl->AddImage(reinterpret_cast<ImTextureID>(fill->Resource),
		p0, p1, ImVec2(u0, v0), ImVec2(u1, v1), col);
	/* Darken Blish fill so cream body text reads like in-game panels. */
	dl->AddRectFilled(p0, p1, IM_COL32(5, 3, 2, static_cast<int>(a * 72.f + 0.5f)));

	dl->PopClipRect();
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

	/* Expanded title — aim at in-game Hero panel header (dark plate + emblem overhang). */
	const float kTitleH = collapsed ? 24.f : 64.f;
	const float kEmblem = collapsed ? 0.f : 78.f; /* hangs past the bar like Hero crest */
	const float kBtn = collapsed ? 18.f : 28.f;
	constexpr float kPadX = 10.f;
	constexpr float kBtnGap = 6.f;
	/* Blish StandardWindow soft edge + always reserve scrollbar gutter. On some
	   Windows hosts the VScroll clip eats the close X if we sit on winW - pad. */
	constexpr float kChromeInsetR = 14.f;

	char vis[96];
	Gw2UiDetail::VisibleLabel(title, vis, sizeof(vis));
	if (char* hash = std::strstr(vis, "##"))
		*hash = '\0';

	const ImVec2 win0 = ImGui::GetWindowPos();
	const float winW = ImGui::GetWindowWidth();
	const ImGuiStyle& style = ImGui::GetStyle();
	/* Prefer live content max (accounts for scroll); fall back to padding+scrollbar. */
	const float contentMaxX = ImGui::GetWindowContentRegionMax().x;
	float rightInset = style.WindowPadding.x + style.ScrollbarSize + kChromeInsetR;
	if (contentMaxX > 80.f && winW - contentMaxX > rightInset)
		rightInset = winW - contentMaxX + kChromeInsetR;
	if (rightInset < style.WindowPadding.x + kChromeInsetR)
		rightInset = style.WindowPadding.x + kChromeInsetR;

	/* Kill padding on the minimized strip so height stays ~one title line. */
	const bool slimPad = collapsed;
	if (slimPad)
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.f, 2.f));

	const ImVec2 row0 = ImGui::GetCursorScreenPos();

	const int nBtns = pOpen ? 2 : 1;
	const float btnsW = static_cast<float>(nBtns) * kBtn + static_cast<float>(nBtns - 1) * kBtnGap + kPadX;
	const float dragW = winW - (row0.x - win0.x) - btnsW - rightInset;
	const float dragWClamped = dragW > 24.f ? dragW : 24.f;

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

		/* Charcoal plate — top strip of StandardWindow fill (Hero-like), then darken. */
		Texture_t* fill = GetChromeTex(static_cast<int>(Icon::PanelFill));
		if (fill && fill->Resource && !collapsed)
		{
			constexpr float tex = 1024.f;
			/* windowRegion top band ≈ title plate inside 155985. */
			const float u0 = 40.f / tex;
			const float u1 = (40.f + 913.f) / tex;
			const float v0 = 26.f / tex;
			const float v1 = (26.f + 78.f) / tex;
			dl->AddImage(reinterpret_cast<ImTextureID>(fill->Resource),
				t0, t1, ImVec2(u0, v0), ImVec2(u1, v1), col);
		}
		dl->AddRectFilled(t0, t1, IM_COL32(8, 7, 6, static_cast<int>(a * (collapsed ? 235.f : 165.f) + 0.5f)));
		/* Subtle ink wash for grain (not the red header brush). */
		if (!collapsed)
		{
			Texture_t* inkTex = GetChromeTex(static_cast<int>(Icon::InkEdge));
			if (inkTex && inkTex->Resource)
			{
				dl->AddImage(reinterpret_cast<ImTextureID>(inkTex->Resource),
					t0, t1,
					ImVec2(0.f, 0.92f), ImVec2(1.f, 0.55f),
					IM_COL32(255, 255, 255, static_cast<int>(a * 55.f + 0.5f)));
			}
		}
		/* Thin frame like the in-game panel header. */
		dl->AddRect(t0, t1, IM_COL32(20, 18, 16, static_cast<int>(a * 220.f + 0.5f)), 0.f, 0, 1.2f);
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
			Texture_t* emblem = GetChromeTex(static_cast<int>(Icon::WindowEmblem));
			/* Emblem hangs past the bar (Hero crest). Allow draw outside title clip. */
			const float ex = row0.x + 4.f;
			const float ey = win0.y + (kTitleH - kEmblem) * 0.5f;
			if (emblem && emblem->Resource)
			{
				dl->AddImage(reinterpret_cast<ImTextureID>(emblem->Resource),
					ImVec2(ex, ey),
					ImVec2(ex + kEmblem, ey + kEmblem),
					ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), col);
				textX = ex + kEmblem * 0.72f; /* title tucks beside crest like Hero */
			}
		}

		/* Hero-panel title: larger cream type + dark halo (not mid brass). */
		ImFont* font = ImGui::GetFont();
		const float baseSz = ImGui::GetFontSize();
		const float titleSz = collapsed ? baseSz : baseSz * 1.55f;
		const ImVec2 tsz = font
			? font->CalcTextSizeA(titleSz, FLT_MAX, 0.f, vis)
			: ImGui::CalcTextSize(vis);
		const float ty = row0.y + (kTitleH - tsz.y) * 0.5f - 1.f;
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

	ImGui::SetCursorScreenPos(ImVec2(win0.x + winW - btnsW - rightInset, row0.y + (kTitleH - kBtn) * 0.5f));
	/* Title controls must paint even when the body work-rect/scrollbar clips. */
	if (dl)
		dl->PushClipRect(win0, ImVec2(win0.x + winW, win0.y + kTitleH + 8.f), false);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.10f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.18f));
	/* In-game close/min have no ImGui frame border — only the glyph/texture. */
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
		const float hw = collapsed ? 5.f : 7.f;
		if (collapsed)
			dl->AddRect(ImVec2(cx - hw, cy - hw), ImVec2(cx + hw, cy + hw), gold, 0.f, 0, 1.6f);
		else
			dl->AddLine(ImVec2(cx - hw, cy), ImVec2(cx + hw, cy), gold, 2.4f);
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
			/* Blish WindowBase2 exit glyphs (Contacts-style X). */
			Texture_t* cxTex = GetChromeNamed(closeHover ? "button-exit-active" : "button-exit");
			if (!cxTex || !cxTex->Resource)
				cxTex = GetChromeNamed("button-exit");
			if (cxTex && cxTex->Resource)
			{
				const float s = kBtn;
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
				const float hw = 6.5f;
				dl->AddLine(ImVec2(cx - hw, cy - hw), ImVec2(cx + hw, cy + hw), gold, 1.8f);
				dl->AddLine(ImVec2(cx + hw, cy - hw), ImVec2(cx - hw, cy + hw), gold, 1.8f);
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

	ImGui::SetCursorScreenPos(ImVec2(row0.x, row0.y + kTitleH + 2.f));
	return true;
}
