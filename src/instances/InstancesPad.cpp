#include "InstancesPad.h"
#include "InstancesInternal.h"

#include "AspectLayout.h"
#include "BrowserTabs.h"
#include "Globals.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "PadDock.h"
#include "PadLayout.h"
#include "Settings.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>

static void OpenWikiSearch(const char* name)
{
	if (!name || !name[0])
		return;
	const std::string enc = WikiBrowser::UrlEncode(name);
	char url[320];
	std::snprintf(url, sizeof(url),
		"https://wiki.guildwars2.com/wiki/Special:Search?search=%s&go=Go", enc.c_str());
	G::ShowWiki = true;
	Settings::SetDirty();
	if (BrowserTabs::OpenNewUrl("wiki", url) < 0)
		WikiBrowser::Navigate(url);
}

bool InstancesPad::Render()
{
	using namespace InstancesDetail;
	if (!G::ShowInstances) return false;
	EnsureCatalog();

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(280.f);
	PadDock::SetSizeConstraints("Instances##GW2InGameHelperInstances", 360.f, 280.f, PadDock::MaxW(540.f), maxH);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.40f) : 150.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.16f) : 110.f;
		PadDock::Place(G::PadInstances, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
	}
	if (!gPlaceOnce && G::PadInstances.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);
	if (gFocus) { ImGui::SetNextWindowFocus(); gFocus = false; }

	bool open = G::ShowInstances;
	HelperTheme::ScopedWindow theme(G::Opacity);
	const bool padBody = ImGui::Begin("Instances##GW2InGameHelperInstances", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("Instances", &open) || !padBody)
	{
		if (PadDock::Capture(G::PadInstances)) Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		HelperTheme::EndPad();
		if (!open) { G::ShowInstances = false; Settings::SetDirty(); }
		return hovered;
	}

	if (!open) { G::ShowInstances = false; Settings::SetDirty(); }
	if (PadDock::Capture(G::PadInstances)) Settings::SetDirty();
	HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);

	static const char* kKinds[] = { "Story", "Fractal", "Raid", "Strike" };
	static const int kKindIcons[] = {
		static_cast<int>(Gw2Ui::Icon::Story),
		static_cast<int>(Gw2Ui::Icon::Map),
		static_cast<int>(Gw2Ui::Icon::PvP),
		static_cast<int>(Gw2Ui::Icon::Squad),
	};
	const int kindIdx = PadNav::DrawSideRail("###gw2igh_inst_nav", kKinds,
		static_cast<int>(Kind::Count), static_cast<int>(gKind), 0.f, kKindIcons);
	if (kindIdx != static_cast<int>(gKind))
	{
		gKind = static_cast<Kind>(kindIdx);
		gSelected = -1;
	}

	ImGui::BeginChild("###gw2igh_inst_body", ImVec2(0.f, 0.f), true);
	TickRaidSync();
	PadNav::Blurb(
		"Sync pulls weekly raids, fractal level, daily fractals, CM achievement overlays, "
		"and story progress via character quests. Strikes stay local (no account strikes API).");
	ImGui::TextColored(HelperTheme::Muted, "%s cleared: %d / %d",
		KindName(gKind), CountCleared(gKind), CountEntries(gKind));
	if (gFractalLevel > 0)
		ImGui::TextColored(HelperTheme::GoldMuted, "Fractal level: %d", gFractalLevel);
	if (!gDailyFractals.empty())
	{
		ImGui::TextColored(HelperTheme::GoldMuted, "Today's fractal dailies:");
		for (const auto& d : gDailyFractals)
			ImGui::BulletText("%s", d.c_str());
	}
	if (ImGui::Button("Sync###gw2igh_inst_sync"))
		StartRaidSync(true);
	ImGui::SameLine();
	if (RaidSyncBusy())
		PadNav::StatusBusy();
	if (ImGui::Button("Reset category###gw2igh_inst_clr"))
		ClearKind(gKind);
	ImGui::SameLine();
	if (ImGui::Button("Reset selected###gw2igh_inst_rs") && gSelected >= 0)
		ResetEntry(static_cast<size_t>(gSelected));
	if (gStatus[0])
		ImGui::TextWrapped("%s", gStatus);
	ImGui::Separator();

	/* Soft re-sync while the pad is open (throttled). */
	StartRaidSync(false);

	/* Keep selection on the active kind; auto-pick first so steps show immediately. */
	if (gSelected >= 0)
	{
		Entry* cur = At(static_cast<size_t>(gSelected));
		if (!cur || cur->kind != gKind)
			gSelected = -1;
	}
	if (gSelected < 0)
	{
		for (size_t i = 0; i < Count(); ++i)
		{
			Entry* e = At(i);
			if (e && e->kind == gKind)
			{
				gSelected = static_cast<int>(i);
				break;
			}
		}
	}

	PadLayout::BeginList("###gw2igh_inst_list");
	int listed = 0;
	for (size_t i = 0; i < Count(); ++i)
	{
		Entry* e = At(i);
		if (!e || e->kind != gKind) continue;
		++listed;
		ImGui::PushID(e->id);
		const bool synced = EntrySynced(*e);
		const bool apiLocked = synced && G::Gw2ApiKey[0] &&
			(e->kind == Kind::Raid || e->kind == Kind::Story || e->achId > 0);
		bool cleared = e->cleared;
		if (apiLocked)
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.55f);
		if (ImGui::Checkbox("##clr", &cleared) && !apiLocked)
			ToggleCleared(i);
		if (apiLocked)
			ImGui::PopStyleVar();
		if (ImGui::IsItemHovered() && synced)
			ImGui::SetTooltip(apiLocked
				? "Synced from API — use Sync to refresh."
				: "Add a progression API key to sync.");
		ImGui::SameLine();
		const int doneSteps = CountStepsDone(i);
		const int totalSteps = static_cast<int>(e->steps.size());
		char label[160];
		std::snprintf(label, sizeof(label), "%s  (%d/%d)%s",
			e->name, doneSteps, totalSteps, e->cleared ? "  [done]" : "");
		const bool sel = gSelected == static_cast<int>(i);
		if (ImGui::Selectable(label, sel))
			gSelected = static_cast<int>(i);
		if (ImGui::IsItemHovered() && e->blurb[0])
			ImGui::SetTooltip("%s", e->blurb);

		if (gSelected == static_cast<int>(i))
		{
			ImGui::Indent();
			if (ImGui::SmallButton("Wiki"))
				OpenWikiSearch(e->name);
			for (size_t s = 0; s < e->steps.size(); ++s)
			{
				bool d = e->steps[s].done;
				char lab[180];
				std::snprintf(lab, sizeof(lab), "%s###st%zu", e->steps[s].text, s);
				const bool stepLocked = apiLocked && StepSynced(e->steps[s]);
				if (stepLocked)
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.55f);
				if (ImGui::Checkbox(lab, &d) && !stepLocked)
					ToggleStep(i, s);
				if (stepLocked)
					ImGui::PopStyleVar();
			}
			ImGui::Unindent();
		}
		ImGui::PopID();
	}
	if (listed == 0)
		ImGui::TextColored(HelperTheme::Muted, "No entries for this category.");
	PadLayout::EndList();
	ImGui::EndChild();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	HelperTheme::EndPad();
	return hovered;
}
