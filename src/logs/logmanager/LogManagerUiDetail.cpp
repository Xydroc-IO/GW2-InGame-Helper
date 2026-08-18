#include "LogManagerShared.h"
#include "LogManagerUiInternal.h"
#include "LogManagerUpload.h"
#include "LogManagerEi.h"

#include "EiRuntime.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <windows.h>
#include <shellapi.h>

namespace LogManagerDetail
{
	void DrawDetailTab(const std::vector<const LogEntry*>& filtered)
	{
		const LogEntry* sel = nullptr;
		if (gSelected >= 0 && gSelected < static_cast<int>(gDraw.size()))
			sel = &gDraw[static_cast<size_t>(gSelected)];

		if (!sel)
		{
			ImGui::TextWrapped("Select a log from the list.");
			return;
		}

		ImGui::TextWrapped("%s", sel->encounter.empty() ? sel->fileName.c_str() : sel->encounter.c_str());
		ImGui::TextColored(HelperTheme::Muted, "%s", sel->fileName.c_str());
		ImGui::Text("Result: %s  Mode: %s  Duration: %s",
			ResultLabel(sel->result),
			sel->mode.empty() ? "Normal" : sel->mode.c_str(),
			FmtDuration(sel->durationMs).c_str());
		ImGui::Text("Time: %s", FmtTime(sel->encounterTime).c_str());
		if (sel->compDps > 0)
			ImGui::Text("Squad DPS: %d", sel->compDps);
		if (!sel->parseError.empty())
			ImGui::TextColored(HelperTheme::Warn, "%s", sel->parseError.c_str());

		if (!sel->encounter.empty())
		{
			std::vector<EncStat> stats;
			BuildEncStats(filtered, stats);
			if (const EncStat* es = FindEncStat(stats, sel->encounter))
			{
				const std::string rate = FmtPct(es->kills, es->attempts);
				ImGui::TextColored(HelperTheme::GoldMuted,
					"This encounter (filtered): %d attempt%s · %d kill%s (%s)",
					es->attempts, es->attempts == 1 ? "" : "s",
					es->kills, es->kills == 1 ? "" : "s",
					rate.c_str());
				if (es->bestKillMs > 0)
				{
					ImGui::SameLine(0.f, 8.f);
					ImGui::TextColored(HelperTheme::Muted, "PB %s",
						FmtDuration(es->bestKillMs).c_str());
					if (sel->result == 1 && sel->durationMs > 0)
					{
						const long long delta = sel->durationMs - es->bestKillMs;
						if (delta == 0)
						{
							ImGui::SameLine(0.f, 6.f);
							ImGui::TextColored(HelperTheme::Ok, "personal best");
						}
						else if (delta > 0)
						{
							ImGui::SameLine(0.f, 6.f);
							ImGui::TextColored(HelperTheme::Muted, "+%s vs PB",
								FmtDuration(delta).c_str());
						}
					}
					if (!es->bestPath.empty() && es->bestPath != sel->pathUtf8)
					{
						ImGui::SameLine(0.f, 8.f);
						if (ImGui::SmallButton("Open PB###gw2igh_lm_openpb"))
							SelectLogByPath(es->bestPath);
					}
				}
			}
		}

		if (ImGui::Button("Parse###gw2igh_lm_parsesel"))
			BeginParseSelected(sel->pathUtf8);
		ImGui::SameLine();
		if (ImGui::Button("Upload###gw2igh_lm_upsel"))
			BeginUpload({sel->pathUtf8});
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Upload this EVTC to dps.report (max 48 MB).");
		ImGui::SameLine();
		if (ImGui::Button("Folder###gw2igh_lm_folder"))
			OpenFolderFor(sel->pathW);

		if (!sel->dpsReportUrl.empty())
		{
			ImGui::TextWrapped("%s", sel->dpsReportUrl.c_str());
			if (ImGui::SmallButton("Open report###gw2igh_lm_openrep"))
				ShellExecuteA(nullptr, "open", sel->dpsReportUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
			ImGui::SameLine();
			if (ImGui::SmallButton("Copy link###gw2igh_lm_copylink"))
				CopyText(sel->dpsReportUrl.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Load DPS/boons###gw2igh_lm_loadstats") && !gHydrateBusy.load())
				BeginHydrateFromReports();
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Squad (DPS + boon uptimes %)");
		if (sel->players.empty())
			ImGui::TextColored(HelperTheme::Muted,
				"No player data - Parse, Upload, or Load DPS/boons.");
		else if (!PlayersHaveDps(sel->players) && !PlayersHaveBoons(sel->players))
		{
			ImGui::TextColored(HelperTheme::Warn,
				"Names loaded - click Load DPS/boons (or Parse with EI).");
			if (ImGui::BeginTable("###gw2igh_lm_squad_basic", 4,
					ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupColumn("Name");
				ImGui::TableSetupColumn("Account");
				ImGui::TableSetupColumn("Prof");
				ImGui::TableSetupColumn("G");
				ImGui::TableHeadersRow();
				for (const PlayerInfo& p : sel->players)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(p.name.c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(p.account.c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(p.profession.c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%d", p.group);
				}
				ImGui::EndTable();
			}
		}
		else if (ImGui::BeginTable("###gw2igh_lm_squad", 12,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
					ImGuiTableFlags_Resizable |
					ImGuiTableFlags_SizingStretchProp,
				ImVec2(-FLT_MIN, -FLT_MIN)))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.6f);
			ImGui::TableSetupColumn("Prof", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("DPS", ImGuiTableColumnFlags_WidthFixed, 56.f);
			ImGui::TableSetupColumn("Pwr", ImGuiTableColumnFlags_WidthFixed, 48.f);
			ImGui::TableSetupColumn("Con", ImGuiTableColumnFlags_WidthFixed, 48.f);
			ImGui::TableSetupColumn("Down", ImGuiTableColumnFlags_WidthFixed, 40.f);
			ImGui::TableSetupColumn("Dead", ImGuiTableColumnFlags_WidthFixed, 40.f);
			ImGui::TableSetupColumn("Quick", ImGuiTableColumnFlags_WidthFixed, 44.f);
			ImGui::TableSetupColumn("Alac", ImGuiTableColumnFlags_WidthFixed, 40.f);
			ImGui::TableSetupColumn("Might", ImGuiTableColumnFlags_WidthFixed, 44.f);
			ImGui::TableSetupColumn("Fury", ImGuiTableColumnFlags_WidthFixed, 40.f);
			ImGui::TableSetupColumn("Prot", ImGuiTableColumnFlags_WidthFixed, 40.f);
			ImGui::TableSetupScrollFreeze(1, 1);
			ImGui::TableHeadersRow();
			auto pct = [](float v) {
				if (v < 0.f)
					return std::string("-");
				char b[16];
				std::snprintf(b, sizeof(b), "%.0f", static_cast<double>(v));
				return std::string(b);
			};
			auto countCell = [](int v) {
				if (v < 0)
					ImGui::TextUnformatted("-");
				else
					ImGui::Text("%d", v);
			};
			for (const PlayerInfo& p : sel->players)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.name.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.profession.c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%d", p.dps);
				ImGui::TableNextColumn();
				ImGui::Text("%d", p.powerDps);
				ImGui::TableNextColumn();
				ImGui::Text("%d", p.condiDps);
				ImGui::TableNextColumn();
				countCell(p.downCount);
				ImGui::TableNextColumn();
				countCell(p.deadCount);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pct(p.quickness).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pct(p.alacrity).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pct(p.might).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pct(p.fury).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pct(p.protection).c_str());
			}
			ImGui::EndTable();
		}
	}

	const LogEntry* SelectedDrawEntry()
	{
		if (gSelected < 0 || gSelected >= static_cast<int>(gDraw.size()))
			return nullptr;
		return &gDraw[static_cast<size_t>(gSelected)];
	}

	std::string GuildLabelFor(const PlayerInfo& p)
	{
		if (!p.guildTag.empty())
			return p.guildTag;
		if (!p.guildId.empty())
		{
			if (p.guildId.size() > 8)
				return p.guildId.substr(0, 8) + "...";
			return p.guildId;
		}
		return {};
	}

	void KickKillProofForSelected(const LogEntry* sel, bool force)
	{
		if (!sel)
			return;
		bool start = false;
		{
			std::lock_guard<std::mutex> lock(gMu);
			for (LogEntry& e : gLogs)
			{
				if (e.pathUtf8 == sel->pathUtf8)
				{
					start = EnsureKillProofForLog(e, force);
					gGen.fetch_add(1);
					break;
				}
			}
		}
		if (start || force)
			BeginKillProofFetch(force);
	}

	void DrawRosterScopeToggle()
	{
		ImGui::RadioButton("This run###gw2igh_lm_scope0", &gRosterScope, 0);
		ImGui::SameLine();
		ImGui::RadioButton("All filtered###gw2igh_lm_scope1", &gRosterScope, 1);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Totals across the logs currently matching Filters (left).");
	}

	void DrawPlayersTab(const std::vector<const LogEntry*>& filtered)
	{
		DrawRosterScopeToggle();
		ImGui::Separator();

		if (gRosterScope == 1)
		{
			std::vector<PlayerAgg> aggs;
			BuildPlayerAggs(filtered, aggs);
			ImGui::TextColored(HelperTheme::Muted,
				"%d players across %d filtered logs  (click a row to search that account)",
				static_cast<int>(aggs.size()), static_cast<int>(filtered.size()));
			if (aggs.empty())
			{
				ImGui::TextWrapped("Parse or Load DPS/boons so account names exist, then filter.");
				return;
			}
			if (ImGui::BeginTable("###gw2igh_lm_paggs_all", 7,
					ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
						ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
					ImVec2(-FLT_MIN, -FLT_MIN)))
			{
				ImGui::TableSetupColumn("Account", ImGuiTableColumnFlags_WidthStretch, 1.5f);
				ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.1f);
				ImGui::TableSetupColumn("Prof", ImGuiTableColumnFlags_WidthStretch, 0.9f);
				ImGui::TableSetupColumn("Logs", ImGuiTableColumnFlags_WidthFixed, 44.f);
				ImGui::TableSetupColumn("Kills", ImGuiTableColumnFlags_WidthFixed, 44.f);
				ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthFixed, 40.f);
				ImGui::TableSetupColumn("Avg DPS", ImGuiTableColumnFlags_WidthStretch, 0.7f);
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableHeadersRow();
				for (const PlayerAgg& a : aggs)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::PushID(a.account.c_str());
					if (ImGui::Selectable(a.account.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
						SetSearch(a.account.c_str());
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Filter the log list to this account.");
					ImGui::PopID();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(a.displayName.empty() ? "-" : a.displayName.c_str());
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(a.profession.empty() ? "-" : a.profession.c_str());
					ImGui::TableNextColumn();
					ImGui::Text("%d", a.logs);
					ImGui::TableNextColumn();
					ImGui::Text("%d", a.success);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(FmtPct(a.success, a.logs).c_str());
					ImGui::TableNextColumn();
					if (a.dpsN > 0)
						ImGui::Text("%d", static_cast<int>(a.dpsSum / a.dpsN));
					else
						ImGui::TextUnformatted("-");
				}
				ImGui::EndTable();
			}
			return;
		}

		const LogEntry* sel = SelectedDrawEntry();
		if (!sel)
		{
			ImGui::TextWrapped("Select a log to see its squad, or switch to All filtered.");
			return;
		}

		ImGui::TextUnformatted(sel->encounter.empty() ? sel->fileName.c_str() : sel->encounter.c_str());
		ImGui::TextColored(HelperTheme::Muted,
			"%d players in this run", static_cast<int>(sel->players.size()));

		if (sel->players.empty())
		{
			ImGui::TextColored(HelperTheme::Warn,
				"No player data - Parse or Load DPS/boons for this log.");
			return;
		}

		if (ImGui::BeginTable("###gw2igh_lm_paggs", 8,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
					ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
				ImVec2(-FLT_MIN, -FLT_MIN)))
		{
			ImGui::TableSetupColumn("Account", ImGuiTableColumnFlags_WidthStretch, 1.4f);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.2f);
			ImGui::TableSetupColumn("Prof", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("DPS", ImGuiTableColumnFlags_WidthStretch, 0.7f);
			ImGui::TableSetupColumn("Down", ImGuiTableColumnFlags_WidthFixed, 40.f);
			ImGui::TableSetupColumn("Dead", ImGuiTableColumnFlags_WidthFixed, 40.f);
			ImGui::TableSetupColumn("Guild", ImGuiTableColumnFlags_WidthStretch, 0.8f);
			ImGui::TableSetupColumn("G", ImGuiTableColumnFlags_WidthStretch, 0.35f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();
			for (const PlayerInfo& p : sel->players)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				if (!p.account.empty())
				{
					ImGui::PushID(p.account.c_str());
					if (ImGui::Selectable(p.account.c_str(), false))
						SetSearch(p.account.c_str());
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Filter the log list to this account.");
					ImGui::PopID();
				}
				else
					ImGui::TextUnformatted("-");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.name.empty() ? "-" : p.name.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.profession.empty() ? "-" : p.profession.c_str());
				ImGui::TableNextColumn();
				if (p.dps > 0)
					ImGui::Text("%d", p.dps);
				else
					ImGui::TextUnformatted("-");
				ImGui::TableNextColumn();
				if (p.downCount < 0)
					ImGui::TextUnformatted("-");
				else
					ImGui::Text("%d", p.downCount);
				ImGui::TableNextColumn();
				if (p.deadCount < 0)
					ImGui::TextUnformatted("-");
				else
					ImGui::Text("%d", p.deadCount);
				ImGui::TableNextColumn();
				const std::string g = GuildLabelFor(p);
				ImGui::TextUnformatted(g.empty() ? "-" : g.c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%d", p.group);
			}
			ImGui::EndTable();
		}
	}

} // namespace LogManagerDetail
