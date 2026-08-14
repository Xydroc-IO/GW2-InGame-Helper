#include "Gw2Ui.h"
#include "Gw2UiInternal.h"

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

#include <windows.h>

namespace
{
	std::unordered_set<int> gRequested;

	struct RailUv { float u0, v0, u1, v1; };

	/* Opaque-content UVs for curated rail PNGs (uneven padding otherwise). */
	bool RailContentUv(int assetId, RailUv& out)
	{
		switch (assetId)
		{
		case 3124871: out = { 11 / 128.f, 4 / 128.f, 118 / 128.f, 118 / 128.f }; return true;
		case 1228855: out = { 4 / 64.f, 2 / 64.f, 62 / 64.f, 64 / 64.f }; return true;
		case 866117: out = { 7 / 64.f, 5 / 64.f, 58 / 64.f, 57 / 64.f }; return true;
		case 156081: out = { 10 / 64.f, 12 / 64.f, 62 / 64.f, 54 / 64.f }; return true;
		case 866115: out = { 9 / 64.f, 11 / 64.f, 54 / 64.f, 54 / 64.f }; return true;
		case 563468: out = { 10 / 128.f, 10 / 128.f, 105 / 128.f, 101 / 128.f }; return true;
		case 561441: out = { 15 / 128.f, 13 / 128.f, 115 / 128.f, 113 / 128.f }; return true;
		case 2199974: out = { 1 / 128.f, 3 / 128.f, 127 / 128.f, 127 / 128.f }; return true;
		case 1228263: out = { 6 / 128.f, 14 / 128.f, 120 / 128.f, 128 / 128.f }; return true;
		case 866124: out = { 6 / 64.f, 8 / 64.f, 59 / 64.f, 57 / 64.f }; return true;
		case 60970: out = { 4 / 64.f, 0 / 64.f, 61 / 64.f, 64 / 64.f }; return true;
		case 155867: out = { 34 / 256.f, 30 / 256.f, 214 / 256.f, 198 / 256.f }; return true;
		case 834008: out = { 2 / 64.f, 2 / 64.f, 62 / 64.f, 62 / 64.f }; return true;
		case 1948130: out = { 6 / 128.f, 2 / 128.f, 128 / 128.f, 128 / 128.f }; return true;
		case 2596974: out = { 4 / 64.f, 5 / 64.f, 59 / 64.f, 61 / 64.f }; return true;
		case 240678: out = { 5 / 128.f, 11 / 128.f, 126 / 128.f, 121 / 128.f }; return true;
		case 3713037: out = { 8 / 128.f, 8 / 128.f, 123 / 128.f, 123 / 128.f }; return true;
		default: return false;
		}
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
			ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::Ink);
		}
	}
}

namespace Gw2UiDetail
{
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
}

void Gw2Ui::Request(int assetId)
{
	if (assetId <= 0 || !G::API)
		return;
	if (!gRequested.insert(assetId).second)
		return;
	char id[48];
	Gw2UiDetail::MakeId(assetId, id, sizeof(id));

	/* Prefer packed ui-chrome PNG (curated Desktop/icons + chrome) over CDN. */
	if (G::API->Textures_GetOrCreateFromFile)
	{
		const std::wstring dataDir = AddonPaths::DataDir();
		if (!dataDir.empty())
		{
			UiChrome::Ensure(dataDir);
			const std::wstring path = UiChrome::PngPath(dataDir, assetId);
			if (!path.empty())
			{
				int n = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
				std::string utf8(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
				if (n > 0)
					WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, utf8.data(), n, nullptr, nullptr);
				if (!utf8.empty())
				{
					G::API->Textures_GetOrCreateFromFile(id, utf8.c_str());
					return;
				}
			}
		}
	}

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
	Request(Icon::BrowseInfo);
	Request(Icon::LedgerCoins);
	Request(Icon::SheetsBook);
	Request(Icon::ApiHourglass);
	Request(Icon::AccountSword);
	Request(Icon::CompassRadar);
	Request(Icon::VaultStar);
	Request(Icon::PathingMap);
	Request(Icon::CompletePeak);
	Request(Icon::FarmSack);
	Request(Icon::Key);
	Request(Icon::EventsMedal);
	Request(Icon::NotesScroll);
	Request(Icon::LogsSwords);
	Request(Icon::EconStack);
	Request(Icon::InstGate);
	Request(Icon::WatchView);
	Request(Icon::SettingsGear);
	Request(Icon::LmPlayers);
	Request(Icon::LmKillProof);
	Request(Icon::LmGuilds);
	Request(Icon::LmFastest);
	Request(Icon::LmDetail);
	Request(Icon::PanelFill);
	Request(Icon::PanelFillAlt);
	Request(Icon::WindowEmblem);
}

bool Gw2Ui::Image(int assetId, float size)
{
	Request(assetId);
	Texture_t* tex = Gw2UiDetail::GetTex(assetId);
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
	Texture_t* tex = Gw2UiDetail::GetTex(assetId);
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
	Texture_t* tex = Gw2UiDetail::GetTex(assetId);
	ImGui::PushID(label);
	bool clicked = false;
	char vis[96];
	Gw2UiDetail::VisibleLabel(label, vis, sizeof(vis));
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

bool Gw2Ui::RailToggle(const char* label, bool on, int assetId, float iconSize, bool showLabel)
{
	if (assetId > 0)
		Request(assetId);
	Texture_t* tex = assetId > 0 ? Gw2UiDetail::GetTex(assetId) : nullptr;
	char vis[96];
	Gw2UiDetail::VisibleLabel(label, vis, sizeof(vis));

	PushRailColors(on);
	bool clicked = false;
	ImGui::PushID(label);
	if (tex)
	{
		const ImGuiStyle& st = ImGui::GetStyle();
		/* Side rail grows FramePadding.y to fill dock height — always honor it. */
		const float padY = st.FramePadding.y * 2.f;
		const float frameH = ImGui::GetFrameHeight();
		const float h = showLabel
			? (frameH > iconSize + padY ? frameH : iconSize + padY)
			: (iconSize + padY);
		clicked = ImGui::Button("##rail", ImVec2(-1.f, h));
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const bool hover = ImGui::IsItemHovered();

		/* Plain fill + gold rim (no stretched btn-frame — those strips top-align badly). */
		const ImU32 accent = ImGui::GetColorU32(
			on ? HelperTheme::GoldBright : HelperTheme::GoldDim);
		dl->AddRectFilled(ImVec2(min.x, min.y), ImVec2(min.x + 3.f, max.y), accent);
		if (on)
			dl->AddRectFilled(
				ImVec2(min.x + 3.f, min.y), ImVec2(max.x, max.y),
				ImGui::GetColorU32(ImVec4(0.94f, 0.77f, 0.35f, 0.06f)));
		{
			const ImU32 goldBorder = ImGui::GetColorU32(
				on ? HelperTheme::GoldBright
				   : (hover ? HelperTheme::Gold : HelperTheme::GoldDim));
			const float thick = on ? 1.75f : (hover ? 1.35f : 1.0f);
			dl->AddRect(min, max, goldBorder, 0.f, 0, thick);
		}

		/* Fit opaque content into a shared slot so padded assets match dense ones. */
		RailUv uv{ 0.f, 0.f, 1.f, 1.f };
		const bool cropped = RailContentUv(assetId, uv);
		const float inset = iconSize * 0.06f;
		const float slot = iconSize - inset * 2.f;
		float dw = slot;
		float dh = slot;
		if (cropped && tex->Width > 0 && tex->Height > 0)
		{
			const float cw = (uv.u1 - uv.u0) * static_cast<float>(tex->Width);
			const float ch = (uv.v1 - uv.v0) * static_cast<float>(tex->Height);
			if (cw > 1.f && ch > 1.f)
			{
				if (cw >= ch)
				{
					dw = slot;
					dh = slot * (ch / cw);
				}
				else
				{
					dh = slot;
					dw = slot * (cw / ch);
				}
			}
		}
		const float iy = min.y + (max.y - min.y - dh) * 0.5f;
		if (showLabel)
		{
			const float ix = min.x + st.FramePadding.x + 3.f + inset + (slot - dw) * 0.5f;
			dl->AddImage(reinterpret_cast<ImTextureID>(tex->Resource),
				ImVec2(ix, iy), ImVec2(ix + dw, iy + dh),
				ImVec2(uv.u0, uv.v0), ImVec2(uv.u1, uv.v1));
			const ImVec2 labelSz = ImGui::CalcTextSize(vis, nullptr, true);
			dl->AddText(ImVec2(min.x + st.FramePadding.x + 3.f + iconSize + st.ItemInnerSpacing.x,
				min.y + (max.y - min.y - labelSz.y) * 0.5f),
				ImGui::GetColorU32(ImGuiCol_Text), vis);
		}
		else
		{
			const float ix = min.x + (max.x - min.x - dw) * 0.5f;
			dl->AddImage(reinterpret_cast<ImTextureID>(tex->Resource),
				ImVec2(ix, iy), ImVec2(ix + dw, iy + dh),
				ImVec2(uv.u0, uv.v0), ImVec2(uv.u1, uv.v1));
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", vis);
		}
	}
	else
		clicked = ImGui::Button(label, ImVec2(-1.f, 0.f));
	ImGui::PopID();
	ImGui::PopStyleColor(4);
	return clicked;
}

bool Gw2Ui::RailToggle(const char* label, bool on, Icon icon, float iconSize, bool showLabel)
{
	return RailToggle(label, on, static_cast<int>(icon), iconSize, showLabel);
}
