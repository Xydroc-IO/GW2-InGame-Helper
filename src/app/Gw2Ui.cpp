#include "Gw2Ui.h"
#include "Gw2UiInternal.h"

#include "Globals.h"
#include "HelperTheme.h"

#include "imgui/imgui.h"
#include "nexus/Nexus.h"

#include <cstdio>
#include <cstring>
#include <unordered_set>

namespace
{
	std::unordered_set<int> gRequested;

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

bool Gw2Ui::RailToggle(const char* label, bool on, int assetId, float iconSize)
{
	if (assetId > 0)
		Request(assetId);
	Texture_t* tex = assetId > 0 ? Gw2UiDetail::GetTex(assetId) : nullptr;
	char vis[96];
	Gw2UiDetail::VisibleLabel(label, vis, sizeof(vis));

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
