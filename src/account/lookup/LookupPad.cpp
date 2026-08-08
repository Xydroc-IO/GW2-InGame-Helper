#include "LookupPad.h"

#include "LookupPadInternal.h"

#include "BrowserTabs.h"
#include "AspectLayout.h"
#include "EconomyInternal.h"
#include "EconomyShared.h"
#include "Globals.h"
#include "Gw2Icons.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "PadDock.h"
#include "Settings.h"
#include "TpWatchPad.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace LookupDetail
{
	std::mutex gMu;
	Hit gHit;
	Hit gPending;
	std::atomic<bool> gBusy{false};
	std::atomic<bool> gReady{false};
	HANDLE gThread = nullptr;
	char gQuery[192] = {};
	char gThreadQuery[192] = {};
	bool gFocus = false;
	bool gPlaceOnce = false;
} // namespace LookupDetail

using namespace LookupDetail;

void LookupPad::OpenAndLookup()
{
	G::ShowLookup = true;
	gFocus = true;
	gPlaceOnce = true;
	Settings::SetDirty();
}

void LookupPad::RenderContents()
{
	Tick();
	Hit hit;
	{
		std::lock_guard<std::mutex> lock(gMu);
		hit = gHit;
	}

	PadNav::Blurb(
		"Chat code (Shift+click), numeric ID, or item name. Official API + wiki - read-only.");

	const float btnW = ImGui::CalcTextSize("Lookup").x + ImGui::GetStyle().FramePadding.x * 2.f + 16.f;
	float fieldW = ImGui::GetContentRegionAvail().x - btnW - ImGui::GetStyle().ItemSpacing.x;
	if (fieldW < 120.f) fieldW = 120.f;
	ImGui::SetNextItemWidth(fieldW);
	if (ImGui::InputTextWithHint("###gw2igh_lookup_q", "[&AgEAAAA=] / ID / name",
			gQuery, sizeof(gQuery), ImGuiInputTextFlags_EnterReturnsTrue))
		StartLookup();
	ImGui::SameLine();
	if (ImGui::Button("Lookup###gw2igh_lookup_go", ImVec2(btnW, 0.f)))
		StartLookup();

	if (gBusy)
		PadNav::StatusBusy("Loading...");
	else if (!hit.status.empty())
		PadNav::StatusOk(hit.status.c_str());

	ImGui::Separator();

	if (hit.ok)
	{
		if (!hit.iconUrl.empty() && Gw2Icons::ImageUrl(hit.iconUrl.c_str(), 40.f))
			ImGui::SameLine(0.f, 10.f);
		ImGui::BeginGroup();
		ImGui::TextColored(RarityColor(hit.rarity), "%s", hit.name.c_str());
		ImGui::TextColored(HelperTheme::Muted, "#%d", hit.id);
		char meta[160];
		std::snprintf(meta, sizeof(meta), "%s%s%s%s%s",
			hit.rarity.c_str(),
			hit.type.empty() ? "" : " | ",
			hit.type.c_str(),
			hit.level.empty() ? "" : " | Lv ",
			hit.level.c_str());
		ImGui::TextUnformatted(meta);
		ImGui::EndGroup();

		if (hit.hasPrices)
		{
			char pl[128];
			std::snprintf(pl, sizeof(pl), "Buy %s | Sell %s",
				FormatCoins(hit.buy).c_str(), FormatCoins(hit.sell).c_str());
			ImGui::TextColored(HelperTheme::Ink, "%s", pl);
		}
		else
			ImGui::TextColored(HelperTheme::Warn, "No TP listings");

		if (ImGui::Button("Wiki###gw2igh_lookup_wiki"))
		{
			char url[384];
			std::snprintf(url, sizeof(url),
				"https://wiki.guildwars2.com/wiki/%s",
				WikiTitleToPath(hit.name).c_str());
			OpenUrl(url);
		}
		ImGui::SameLine();
		if (ImGui::Button("BLTC###gw2igh_lookup_bltc"))
		{
			char url[128];
			std::snprintf(url, sizeof(url), "https://www.gw2bltc.com/en/item/%d", hit.id);
			OpenUrl(url);
		}
		ImGui::SameLine();
		if (ImGui::Button("Add to TP###gw2igh_lookup_tp"))
		{
			std::string st;
			TpWatchPad::AddItem(hit.id, &st);
			TpWatchPad::OpenAndRefresh();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cart###gw2igh_lookup_cart"))
		{
			EconomyDetail::AddToCart(hit.id, hit.name.empty() ? "Item" : hit.name.c_str(), 1);
			G::ShowEconomy = true;
			EconomyDetail::gTab = EconomyDetail::kTabCart;
			EconomyDetail::gForceTab = EconomyDetail::kTabCart;
			Settings::SetDirty();
		}
	}
	else if (!hit.nameHints.empty())
	{
		PadNav::Meta("Wiki results");
		for (size_t i = 0; i < hit.nameHints.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::Selectable(hit.nameHints[i].c_str()))
			{
				char url[384];
				std::snprintf(url, sizeof(url),
					"https://wiki.guildwars2.com/wiki/%s",
					WikiTitleToPath(hit.nameHints[i]).c_str());
				OpenUrl(url);
			}
			ImGui::PopID();
		}
	}

}

bool LookupPad::Render()
{
	if (!G::ShowLookup)
		return false;

	const ImGuiIO& io = ImGui::GetIO();
	constexpr float kPadW = PadDock::kCompactW;
	constexpr float kPadH = PadDock::kCompactH;
	PadDock::SetSizeConstraints("Item Lookup##GW2InGameHelperLookup", 360.f, 200.f,
		PadDock::MaxW(520.f), PadDock::MaxH(280.f));
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.42f) : 120.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.18f) : 100.f;
		PadDock::Place(G::PadLookup, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy), /*applySize=*/true);
	}
	if (!gPlaceOnce && G::PadLookup.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);
	if (gFocus)
	{
		ImGui::SetNextWindowFocus();
		gFocus = false;
	}

	bool open = G::ShowLookup;
	HelperTheme::ScopedWindow theme(G::Opacity);
	const bool padBody = ImGui::Begin("Item Lookup##GW2InGameHelperLookup", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("Item Lookup", &open) || !padBody)
	{
		if (PadDock::Capture(G::PadLookup))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		HelperTheme::EndPad();
		if (!open)
		{
			G::ShowLookup = false;
			Settings::SetDirty();
		}
		return hovered;
	}

	if (!open)
	{
		G::ShowLookup = false;
		Settings::SetDirty();
	}
	if (PadDock::Capture(G::PadLookup))
		Settings::SetDirty();

	HelperTheme::ScopedFontScale fontScale(PadDock::kCompactW, PadDock::kCompactH);
	RenderContents();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	HelperTheme::EndPad();
	return hovered;
}
