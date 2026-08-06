#include "LookupPad.h"

#include "LookupPadInternal.h"

#include "BrowserTabs.h"
#include "AspectLayout.h"
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

	ImGui::TextUnformatted("Item lookup");
	PadNav::PushWrap();
	ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
		"Chat code (Shift+click), numeric ID, or item name. Official API + wiki - read-only.");
	PadNav::PopWrap();

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
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Loading...");
	else if (!hit.status.empty())
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", hit.status.c_str());

	ImGui::Separator();

	if (hit.ok)
	{
		if (!hit.iconUrl.empty() && Gw2Icons::ImageUrl(hit.iconUrl.c_str(), 40.f))
			ImGui::SameLine(0.f, 10.f);
		ImGui::BeginGroup();
		ImGui::TextColored(RarityColor(hit.rarity), "%s", hit.name.c_str());
		ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "#%d", hit.id);
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
			ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.84f, 1.f), "%s", pl);
		}
		else
			ImGui::TextColored(ImVec4(0.70f, 0.55f, 0.40f, 1.f), "No TP listings");

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
			std::string csv = G::TpWatchIds;
			char idBuf[24];
			std::snprintf(idBuf, sizeof(idBuf), "%d", hit.id);
			bool already = false;
			{
				/* Token-aware check so 21 does not match 19721. */
				const char* p = csv.c_str();
				while (*p)
				{
					while (*p == ',' || *p == ' ') ++p;
					int v = 0;
					bool any = false;
					while (*p >= '0' && *p <= '9')
					{
						any = true;
						v = v * 10 + (*p - '0');
						++p;
					}
					if (any && v == hit.id) { already = true; break; }
					while (*p && *p != ',') ++p;
				}
			}
			if (!already)
			{
				if (!csv.empty() && csv.back() != ',') csv += ',';
				csv += idBuf;
				if (csv.size() < sizeof(G::TpWatchIds))
				{
					std::snprintf(G::TpWatchIds, sizeof(G::TpWatchIds), "%s", csv.c_str());
					Settings::SetDirty();
				}
			}
			TpWatchPad::OpenAndRefresh();
		}
	}
	else if (!hit.nameHints.empty())
	{
		ImGui::TextUnformatted("Wiki results");
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
	ImGui::SetNextWindowSizeConstraints(ImVec2(360.f, 200.f),
		ImVec2(PadDock::MaxW(520.f), PadDock::MaxH(280.f)));
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.42f) : 120.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.18f) : 100.f;
		PadDock::Place(G::PadLookup, gPlaceOnce, 440.f, 420.f, ImVec2(fx, fy), /*applySize=*/true);
	}
	if (!gPlaceOnce && G::PadLookup.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(440.f, 420.f), ImGuiCond_FirstUseEver);
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
		ImGui::End();
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

	HelperTheme::ScopedFontScale fontScale;
	RenderContents();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	ImGui::End();
	return hovered;
}
