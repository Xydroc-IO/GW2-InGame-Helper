#include "Gw2Ui.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "UiChrome.h"

#include "imgui/imgui.h"
#include "nexus/Nexus.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>

namespace
{
	std::unordered_set<int> gRequested;

	void MakeId(int assetId, char* out, size_t outLen)
	{
		std::snprintf(out, outLen, "GW2IGH_UI_%d", assetId);
	}

	Texture_t* GetTex(int assetId)
	{
		if (assetId <= 0 || !G::API || !G::API->Textures_Get)
			return nullptr;
		char id[48];
		MakeId(assetId, id, sizeof(id));
		Texture_t* tex = G::API->Textures_Get(id);
		if (!tex || !tex->Resource)
			return nullptr;
		return tex;
	}

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
		return GetTex(assetId);
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

	/* Visible text before ### (ImGui id separator). */
	void VisibleLabel(const char* label, char* out, size_t outLen)
	{
		if (!label || !out || outLen == 0)
		{
			if (out && outLen)
				out[0] = '\0';
			return;
		}
		const char* hash = std::strstr(label, "###");
		size_t n = hash ? static_cast<size_t>(hash - label) : std::strlen(label);
		if (n >= outLen)
			n = outLen - 1;
		std::memcpy(out, label, n);
		out[n] = '\0';
	}

	void PushRailColors(bool on)
	{
		if (on)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, HelperTheme::TabActive);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HelperTheme::TabActive);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, HelperTheme::TabActive);
			ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::GoldBright);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, HelperTheme::TabIdle);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HelperTheme::Header);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, HelperTheme::TabActive);
			ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::Muted);
		}
	}
}

void Gw2Ui::Request(int assetId)
{
	if (assetId <= 0 || !G::API)
		return;
	if (!gRequested.insert(assetId).second)
		return;
	char id[48];
	MakeId(assetId, id, sizeof(id));
	char endpoint[48];
	std::snprintf(endpoint, sizeof(endpoint), "/%d.png", assetId);
	if (G::API->Textures_GetOrCreateFromURL)
		G::API->Textures_GetOrCreateFromURL(id, "https://assets.gw2dat.com", endpoint);
}

void Gw2Ui::WarmCommon()
{
	Request(Icon::Close);
	Request(Icon::Check);
	Request(Icon::Cancel);
	Request(Icon::Alert);
	Request(Icon::GoldCoins);
	Request(Icon::Gem);
	Request(Icon::Achievements);
	Request(Icon::Story);
	Request(Icon::Back);
	Request(Icon::LockBag);
	Request(Icon::Bag);
	Request(Icon::Hero);
	Request(Icon::Trade);
	Request(Icon::Inventory);
	Request(Icon::Mail);
	Request(Icon::Options);
	Request(Icon::Contacts);
	Request(Icon::Squad);
	Request(Icon::PvP);
	Request(Icon::Map);
	Request(Icon::Help);
	Request(Icon::PanelFill);
	Request(Icon::PanelFillAlt);
	Request(Icon::WindowEmblem);
}

bool Gw2Ui::Image(int assetId, float size)
{
	Request(assetId);
	Texture_t* tex = GetTex(assetId);
	if (!tex)
		return false;
	ImGui::Image(reinterpret_cast<ImTextureID>(tex->Resource), ImVec2(size, size));
	return true;
}

bool Gw2Ui::Image(Icon icon, float size)
{
	return Image(static_cast<int>(icon), size);
}

bool Gw2Ui::IconButton(const char* id, int assetId, float size)
{
	Request(assetId);
	Texture_t* tex = GetTex(assetId);
	ImGui::PushID(id);
	bool clicked = false;
	if (tex)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.12f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.22f));
		clicked = ImGui::ImageButton(
			reinterpret_cast<ImTextureID>(tex->Resource),
			ImVec2(size, size),
			ImVec2(0, 0), ImVec2(1, 1),
			0,
			ImVec4(0, 0, 0, 0),
			ImVec4(1, 1, 1, 1));
		ImGui::PopStyleColor(3);
	}
	else
	{
		char buf[32];
		std::snprintf(buf, sizeof(buf), "...###%s", id);
		clicked = ImGui::Button(buf, ImVec2(size + 4.f, size + 4.f));
	}
	ImGui::PopID();
	return clicked;
}

bool Gw2Ui::IconButton(const char* id, Icon icon, float size)
{
	return IconButton(id, static_cast<int>(icon), size);
}

bool Gw2Ui::IconLabelButton(const char* label, int assetId, float iconSize)
{
	Request(assetId);
	Texture_t* tex = GetTex(assetId);
	ImGui::PushID(label);
	bool clicked = false;
	char vis[96];
	VisibleLabel(label, vis, sizeof(vis));
	if (tex)
	{
		const ImGuiStyle& st = ImGui::GetStyle();
		const float h = ImGui::GetFrameHeight();
		const ImVec2 labelSz = ImGui::CalcTextSize(vis, nullptr, true);
		const float w = iconSize + st.ItemInnerSpacing.x + labelSz.x + st.FramePadding.x * 2.f;
		clicked = ImGui::Button("##ibg", ImVec2(w, h > iconSize + 4.f ? h : iconSize + 4.f));
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float iy = min.y + (max.y - min.y - iconSize) * 0.5f;
		dl->AddImage(reinterpret_cast<ImTextureID>(tex->Resource),
			ImVec2(min.x + st.FramePadding.x, iy),
			ImVec2(min.x + st.FramePadding.x + iconSize, iy + iconSize));
		dl->AddText(ImVec2(min.x + st.FramePadding.x + iconSize + st.ItemInnerSpacing.x,
			min.y + (max.y - min.y - labelSz.y) * 0.5f),
			ImGui::GetColorU32(ImGuiCol_Text), vis);
	}
	else
		clicked = ImGui::Button(label);
	ImGui::PopID();
	return clicked;
}

bool Gw2Ui::IconLabelButton(const char* label, Icon icon, float iconSize)
{
	return IconLabelButton(label, static_cast<int>(icon), iconSize);
}

bool Gw2Ui::RailToggle(const char* label, bool on, int assetId, float iconSize)
{
	if (assetId > 0)
		Request(assetId);
	Texture_t* tex = assetId > 0 ? GetTex(assetId) : nullptr;
	char vis[96];
	VisibleLabel(label, vis, sizeof(vis));

	PushRailColors(on);
	bool clicked = false;
	if (tex)
	{
		const ImGuiStyle& st = ImGui::GetStyle();
		const float frameH = ImGui::GetFrameHeight();
		const float h = frameH > iconSize + st.FramePadding.y * 2.f
			? frameH : iconSize + st.FramePadding.y * 2.f;
		ImGui::PushID(label);
		clicked = ImGui::Button("##rail", ImVec2(-1.f, h));
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		/* Gold plaque rail accent — selected bright, idle dim hairline. */
		const ImU32 accent = ImGui::GetColorU32(
			on ? HelperTheme::GoldBright : HelperTheme::GoldDim);
		dl->AddRectFilled(ImVec2(min.x, min.y), ImVec2(min.x + 3.f, max.y), accent);
		if (on)
			dl->AddRectFilled(
				ImVec2(min.x + 3.f, min.y), ImVec2(max.x, max.y),
				ImGui::GetColorU32(ImVec4(0.94f, 0.77f, 0.35f, 0.06f)));
		const float iy = min.y + (max.y - min.y - iconSize) * 0.5f;
		const float ix = min.x + st.FramePadding.x + 3.f;
		dl->AddImage(reinterpret_cast<ImTextureID>(tex->Resource),
			ImVec2(ix, iy), ImVec2(ix + iconSize, iy + iconSize));
		const ImVec2 labelSz = ImGui::CalcTextSize(vis, nullptr, true);
		dl->AddText(ImVec2(ix + iconSize + st.ItemInnerSpacing.x,
			min.y + (max.y - min.y - labelSz.y) * 0.5f),
			ImGui::GetColorU32(ImGuiCol_Text), vis);
		ImGui::PopID();
	}
	else
		clicked = ImGui::Button(label, ImVec2(-1.f, 0.f));
	ImGui::PopStyleColor(4);
	return clicked;
}

bool Gw2Ui::RailToggle(const char* label, bool on, Icon icon, float iconSize)
{
	return RailToggle(label, on, static_cast<int>(icon), iconSize);
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

	/* Expanded title vs thin minimized strip (old ImGui collapsed-bar scale). */
	const float kTitleH = collapsed ? 20.f : 50.f;
	const float kEmblem = collapsed ? 0.f : 32.f;
	const float kBtn = collapsed ? 16.f : 24.f;
	constexpr float kPadX = 8.f;
	constexpr float kBtnGap = 2.f;
	/* Keep controls inside ImGui's content clip (WindowPadding) + Blish frame edge. */
	constexpr float kChromeInsetR = 6.f;

	char vis[96];
	VisibleLabel(title, vis, sizeof(vis));
	if (char* hash = std::strstr(vis, "##"))
		*hash = '\0';

	const ImVec2 win0 = ImGui::GetWindowPos();
	const float winW = ImGui::GetWindowWidth();
	const float padR = ImGui::GetStyle().WindowPadding.x + kChromeInsetR;

	/* Kill padding on the minimized strip so height stays ~one title line. */
	const bool slimPad = collapsed;
	if (slimPad)
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.f, 2.f));

	const ImVec2 row0 = ImGui::GetCursorScreenPos();

	const int nBtns = pOpen ? 2 : 1;
	const float btnsW = static_cast<float>(nBtns) * kBtn + static_cast<float>(nBtns - 1) * kBtnGap + kPadX;
	const float dragW = winW - (row0.x - win0.x) - btnsW - padR;
	const float dragWClamped = dragW > 24.f ? dragW : 24.f;

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImU32 col = IM_COL32(255, 255, 255, static_cast<int>(a * 255.f + 0.5f));
	const ImU32 gold = ImGui::GetColorU32(ImVec4(
		HelperTheme::Gold.x, HelperTheme::Gold.y, HelperTheme::Gold.z, a));

	if (collapsed && dl)
	{
		/* Slim bar — not the stretched StandardWindow texture. */
		const ImVec2 p1(win0.x + winW, win0.y + kTitleH + 4.f);
		dl->AddRectFilled(win0, p1, IM_COL32(18, 14, 10, static_cast<int>(a * 245.f + 0.5f)));
		dl->AddRect(win0, p1, IM_COL32(161, 120, 56, static_cast<int>(a * 180.f + 0.5f)));
		dl->AddLine(
			ImVec2(win0.x + 1.f, win0.y + kTitleH + 3.f),
			ImVec2(win0.x + winW - 1.f, win0.y + kTitleH + 3.f),
			IM_COL32(180, 140, 60, static_cast<int>(a * 70.f + 0.5f)));
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
			const float ey = row0.y + (kTitleH - kEmblem) * 0.5f;
			if (emblem && emblem->Resource)
			{
				dl->AddImage(reinterpret_cast<ImTextureID>(emblem->Resource),
					ImVec2(row0.x + 6.f, ey),
					ImVec2(row0.x + 6.f + kEmblem, ey + kEmblem),
					ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), col);
				textX = row0.x + 6.f + kEmblem + 6.f;
			}
		}

		const ImVec2 tsz = ImGui::CalcTextSize(vis);
		const float ty = row0.y + (kTitleH - tsz.y) * 0.5f;
		dl->AddText(ImVec2(textX, ty), gold, vis);
	}

	ImGui::SetCursorScreenPos(ImVec2(win0.x + winW - btnsW - padR, row0.y + (kTitleH - kBtn) * 0.5f));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.12f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.22f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));

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
		const float hw = collapsed ? 3.5f : 4.5f;
		if (collapsed)
			dl->AddRect(ImVec2(cx - hw, cy - hw), ImVec2(cx + hw, cy + hw), gold, 0.f, 0, 1.4f);
		else
			dl->AddLine(ImVec2(cx - hw, cy), ImVec2(cx + hw, cy), gold, 2.f);
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
			/* Blish WindowBase2: button-exit / button-exit-active (Contacts-style X). */
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
				const float hw = 3.5f;
				dl->AddLine(ImVec2(cx - hw, cy - hw), ImVec2(cx + hw, cy + hw), gold, 1.6f);
				dl->AddLine(ImVec2(cx + hw, cy - hw), ImVec2(cx - hw, cy + hw), gold, 1.6f);
			}
		}
	}

	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);

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
