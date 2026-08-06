#include "InstancesPad.h"
#include "InstancesInternal.h"

#include "AspectLayout.h"
#include "BrowserTabs.h"
#include "Globals.h"
#include "HelperTheme.h"
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
	ImGui::SetNextWindowSizeConstraints(ImVec2(360.f, 280.f), ImVec2(PadDock::MaxW(540.f), maxH));
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
	if (!ImGui::Begin("Instances##GW2InGameHelperInstances", &open))
	{
		if (PadDock::Capture(G::PadInstances)) Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		ImGui::End();
		if (!open) { G::ShowInstances = false; Settings::SetDirty(); }
		return hovered;
	}
	if (!open) { G::ShowInstances = false; Settings::SetDirty(); }
	if (PadDock::Capture(G::PadInstances)) Settings::SetDirty();
	HelperTheme::ScopedFontScale fontScale;

	ImGui::TextColored(HelperTheme::Gold, "INSTANCES");
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextColored(HelperTheme::Muted,
		"Local journals — ticks save on disk. Not live weekly API.");
	ImGui::PopTextWrapPos();

	const float rowW = ImGui::GetContentRegionAvail().x;
	float used = 0.f;
	for (int i = 0; i < static_cast<int>(Kind::Count); ++i)
	{
		const Kind k = static_cast<Kind>(i);
		const float need = ImGui::CalcTextSize(KindName(k)).x +
			ImGui::GetStyle().FramePadding.x * 2.f + ImGui::GetStyle().ItemSpacing.x;
		if (i > 0 && used + need > rowW)
			used = 0.f;
		else if (i > 0)
			ImGui::SameLine();
		if (ImGui::RadioButton(KindName(k), gKind == k))
		{
			gKind = k;
			gSelected = -1;
		}
		used += need;
	}

	ImGui::TextColored(HelperTheme::Muted, "%s cleared: %d / %d",
		KindName(gKind), CountCleared(gKind), CountEntries(gKind));
	if (ImGui::Button("Reset category###gw2igh_inst_clr"))
		ClearKind(gKind);
	ImGui::SameLine();
	if (ImGui::Button("Reset selected###gw2igh_inst_rs") && gSelected >= 0)
		ResetEntry(static_cast<size_t>(gSelected));
	if (gStatus[0])
		ImGui::TextWrapped("%s", gStatus);
	ImGui::Separator();

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
		bool cleared = e->cleared;
		if (ImGui::Checkbox("##clr", &cleared))
			ToggleCleared(i);
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
				if (ImGui::Checkbox(lab, &d))
					ToggleStep(i, s);
			}
			ImGui::Unindent();
		}
		ImGui::PopID();
	}
	if (listed == 0)
		ImGui::TextColored(HelperTheme::Muted, "No entries for this category.");
	PadLayout::EndList();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	ImGui::End();
	return hovered;
}
