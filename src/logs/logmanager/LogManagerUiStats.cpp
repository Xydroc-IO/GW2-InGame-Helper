#include "LogManagerShared.h"

#include "HelperTheme.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <string>
#include <vector>

namespace LogManagerDetail
{
	void DrawStatsTab(const std::vector<const LogEntry*>& filtered)
	{
		int kills = 0, fails = 0, unknown = 0, uploaded = 0, parsed = 0;
		for (const LogEntry* e : filtered)
		{
			if (!e)
				continue;
			if (e->result == 1)
				++kills;
			else if (e->result == 0)
				++fails;
			else
				++unknown;
			if (!e->dpsReportUrl.empty())
				++uploaded;
			if (e->state != ParseState::Pending || !e->encounter.empty())
				++parsed;
		}
		const int n = static_cast<int>(filtered.size());
		ImGui::TextUnformatted("Overview (filtered)");
		ImGui::TextColored(HelperTheme::Muted,
			"%d logs  ·  %d kills (%s)  ·  %d fails  ·  %d unknown  ·  %d parsed  ·  %d uploaded",
			n, kills, FmtPct(kills, n).c_str(), fails, unknown, parsed, uploaded);
		ImGui::TextColored(HelperTheme::GoldMuted,
			"Click an encounter to open the latest log. PB opens the personal-best kill.");
		ImGui::Separator();

		std::vector<EncStat> stats;
		BuildEncStats(filtered, stats);
		if (stats.empty())
		{
			ImGui::TextWrapped("No logs match filters.");
			return;
		}

		if (ImGui::BeginTable("###gw2igh_lm_encstats", 8,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
					ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
				ImVec2(-FLT_MIN, -FLT_MIN)))
		{
			ImGui::TableSetupColumn("Encounter", ImGuiTableColumnFlags_WidthStretch, 1.8f);
			ImGui::TableSetupColumn("N", ImGuiTableColumnFlags_WidthFixed, 36.f);
			ImGui::TableSetupColumn("Kills", ImGuiTableColumnFlags_WidthFixed, 44.f);
			ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthFixed, 40.f);
			ImGui::TableSetupColumn("Best", ImGuiTableColumnFlags_WidthStretch, 0.7f);
			ImGui::TableSetupColumn("Avg kill", ImGuiTableColumnFlags_WidthStretch, 0.7f);
			ImGui::TableSetupColumn("Squad PB", ImGuiTableColumnFlags_WidthStretch, 0.7f);
			ImGui::TableSetupColumn("Last", ImGuiTableColumnFlags_WidthStretch, 1.1f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();
			for (const EncStat& s : stats)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::PushID(s.encounter.c_str());
				if (ImGui::Selectable(s.encounter.c_str(), false))
				{
					if (!s.latestPath.empty())
						SelectLogAndShowDetail(s.latestPath);
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Open latest log of this encounter.");
				ImGui::PopID();
				ImGui::TableNextColumn();
				ImGui::Text("%d", s.attempts);
				ImGui::TableNextColumn();
				ImGui::Text("%d", s.kills);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(FmtPct(s.kills, s.attempts).c_str());
				ImGui::TableNextColumn();
				if (s.bestKillMs > 0)
				{
					ImGui::PushID("pb");
					ImGui::PushID(s.encounter.c_str());
					const std::string best = FmtDuration(s.bestKillMs);
					if (ImGui::SmallButton(best.c_str()))
					{
						if (!s.bestPath.empty())
							SelectLogAndShowDetail(s.bestPath);
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Open personal-best kill.");
					ImGui::PopID();
					ImGui::PopID();
				}
				else
					ImGui::TextUnformatted("-");
				ImGui::TableNextColumn();
				if (s.killDurN > 0)
					ImGui::TextUnformatted(FmtDuration(s.killDurSum / s.killDurN).c_str());
				else
					ImGui::TextUnformatted("-");
				ImGui::TableNextColumn();
				if (s.bestSquadDps > 0)
					ImGui::Text("%d", s.bestSquadDps);
				else
					ImGui::TextUnformatted("-");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(FmtTime(s.lastTime).c_str());
			}
			ImGui::EndTable();
		}
	}
} // namespace LogManagerDetail
