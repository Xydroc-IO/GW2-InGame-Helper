#include "CompletionShared.h"

#include "HelperTheme.h"
#include "PadNav.h"

#include "imgui/imgui.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace CompletionDetail
{
	namespace
	{
		std::string ToLowerCopy(std::string s)
		{
			for (char& c : s)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return s;
		}

		void CatProgress(const AchCategory& c, int& done, int& total)
		{
			done = 0;
			total = static_cast<int>(c.achievementIds.size());
			for (int id : c.achievementIds)
			{
				ApProgress p{};
				if (LookupApProgress(static_cast<uint32_t>(id), p) && p.done)
					++done;
			}
		}
	}

	void DrawAchievementsTab()
	{
		ApplyApOverlayResult();
		ApplyAchCatalogResult();
		ApplyAchDefsResult();
		ApplyAchWikiThumbResult();
		LoadAchPins();
		BeginAchCatalogRefresh(false);
		if (!G::Gw2ApiKey[0])
			PadNav::Blurb("Add a GW2 API key with progression in Settings.");
		else
			PadNav::Blurb("Groups come from the catalog pack; the API only syncs your progress.");

		if (PadNav::RefreshButton("###gw2igh_ap_api"))
		{
			BeginApOverlayRefresh();
			BeginAchCatalogRefresh(true);
		}
		ImGui::SameLine();
		if (ApOverlayBusy() || AchCatalogBusy())
			PadNav::StatusBusy("Loading...");
		else
			ImGui::TextColored(HelperTheme::Muted, "%d on account",
				static_cast<int>(ApProgressCount()));

		if (AchCatalogBusy() && !AchCatalogReady())
			return;
		if (!AchCatalogReady())
		{
			ImGui::TextColored(HelperTheme::Warn, "Could not load achievement groups.");
			return;
		}

		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::InputTextWithHint("###gw2igh_ap_q", "Search this category",
			gApSearch, sizeof(gApSearch));
		if (ImGui::SmallButton("All###gw2igh_ap_fa"))
			gApFilter = 0;
		ImGui::SameLine();
		if (ImGui::SmallButton("Done###gw2igh_ap_fd"))
			gApFilter = 1;
		ImGui::SameLine();
		if (ImGui::SmallButton("Open###gw2igh_ap_fo"))
			gApFilter = 2;

		PadNav::SectionTitle("Working on");
		{
			const std::vector<int>& pins = AchPins();
			if (pins.empty())
				ImGui::TextColored(HelperTheme::Muted, "Pin up to %d from the detail below.",
					kMaxAchPins);
			else
			{
				for (int pid : pins)
				{
					ImGui::PushID(pid + 900000);
					const AchDef* pd = FindAchDef(pid);
					char lab[160];
					if (pd && !pd->name.empty())
						std::snprintf(lab, sizeof(lab), "%s###gw2igh_ap_pw", pd->name.c_str());
					else
						std::snprintf(lab, sizeof(lab), "#%d###gw2igh_ap_pw", pid);
					if (ImGui::SmallButton(lab))
						FocusAchPin(pid);
					ImGui::SameLine();
					if (ImGui::SmallButton("x###gw2igh_ap_px"))
						ToggleAchPin(pid);
					ImGui::PopID();
				}
			}
		}

		static int sDefsFor = -1;
		if (gApSelCatId > 0 && sDefsFor != gApSelCatId && !AchDefsBusy())
		{
			sDefsFor = gApSelCatId;
			BeginAchDefsRefresh(gApSelCatId);
		}

		ImGui::NewLine();
		const float splitAvail = ImGui::GetContentRegionAvail().x;
		const float splitGap = ImGui::GetStyle().ItemSpacing.x;
		const float treeW = splitAvail * 0.42f;
		const float listW = splitAvail - treeW - splitGap;
		const float splitH = ImGui::GetContentRegionAvail().y * 0.42f;
		ImGui::BeginChild("###gw2igh_ap_acctree", ImVec2(treeW, splitH), true);
		for (const AchGroup& g : AchGroups())
		{
			ImGui::PushID(g.id.c_str());
			if (ImGui::TreeNodeEx("##g", ImGuiTreeNodeFlags_SpanAvailWidth, "%s",
				g.name.c_str()))
			{
				for (int cid : g.categoryIds)
				{
					const AchCategory* c = FindAchCategory(cid);
					if (!c)
						continue;
					int done = 0, total = 0;
					CatProgress(*c, done, total);
					char lab[160];
					std::snprintf(lab, sizeof(lab), "%s  %d/%d",
						c->name.empty() ? "?" : c->name.c_str(), done, total);
					if (ImGui::Selectable(lab, gApSelCatId == c->id))
					{
						gApSelCatId = c->id;
						gApSelAchId = 0;
					}
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
		ImGui::SameLine(0.f, splitGap);
		ImGui::BeginChild("###gw2igh_ap_acclist", ImVec2(listW, splitH), true);
		const AchCategory* cat = FindAchCategory(gApSelCatId);
		if (!cat)
			ImGui::TextColored(HelperTheme::Muted, "Pick a category.");
		else
		{
			if (AchDefsBusy())
				PadNav::StatusBusy("Loading names...");
			const std::string q = ToLowerCopy(gApSearch);
			int shown = 0;
			int firstId = 0;
			for (int id : cat->achievementIds)
			{
				ApProgress p{};
				const bool known = LookupApProgress(static_cast<uint32_t>(id), p);
				const bool done = known && p.done;
				if (gApFilter == 1 && !done)
					continue;
				if (gApFilter == 2 && done)
					continue;
				const AchDef* d = FindAchDef(id);
				const char* nm = (d && !d->name.empty()) ? d->name.c_str() : nullptr;
				char fallback[32];
				if (!nm)
				{
					std::snprintf(fallback, sizeof(fallback), "#%d", id);
					nm = fallback;
				}
				if (!q.empty() && ToLowerCopy(nm).find(q) == std::string::npos)
					continue;
				if (firstId == 0)
					firstId = id;
				++shown;
				ImGui::PushID(id);
				char row[192];
				if (done)
					std::snprintf(row, sizeof(row), "%s", nm);
				else if (known && p.max > 0)
					std::snprintf(row, sizeof(row), "%s  %d/%d", nm, p.current, p.max);
				else
					std::snprintf(row, sizeof(row), "%s", nm);
				if (done)
					ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::Ok);
				if (ImGui::Selectable(row, gApSelAchId == id))
					gApSelAchId = id;
				if (done)
					ImGui::PopStyleColor();
				ImGui::PopID();
			}
			if (gApSelAchId == 0 && firstId != 0)
				gApSelAchId = firstId;
			if (shown == 0)
				ImGui::TextColored(HelperTheme::Muted, "Nothing in this filter.");
		}
		ImGui::EndChild();

		ImGui::BeginChild("###gw2igh_ap_detail", ImVec2(0.f, 0.f), true,
			ImGuiWindowFlags_AlwaysVerticalScrollbar);
		if (gApSelAchId > 0)
			DrawAchievementDetail(gApSelAchId);
		else
			ImGui::TextColored(HelperTheme::Muted,
				"Click an achievement above to see how to complete it.");
		ImGui::EndChild();
	}
}
