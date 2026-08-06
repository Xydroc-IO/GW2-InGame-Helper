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
	void DrawDetailTab()
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
			ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.40f, 1.f), "%s", sel->parseError.c_str());

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
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f),
				"Names loaded - click Load DPS/boons (or Parse with EI).");
			if (ImGui::BeginTable("###gw2igh_lm_squad_basic", 4,
					ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
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
		else if (ImGui::BeginTable("###gw2igh_lm_squad", 10,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
					ImGuiTableFlags_ScrollX | ImGuiTableFlags_Resizable |
					ImGuiTableFlags_SizingStretchProp,
				ImVec2(-FLT_MIN, -FLT_MIN)))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.6f);
			ImGui::TableSetupColumn("Prof", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("DPS", ImGuiTableColumnFlags_WidthFixed, 56.f);
			ImGui::TableSetupColumn("Pwr", ImGuiTableColumnFlags_WidthFixed, 48.f);
			ImGui::TableSetupColumn("Con", ImGuiTableColumnFlags_WidthFixed, 48.f);
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

	void DrawPlayersTab(const std::vector<const LogEntry*>& /*filtered*/)
	{
		const LogEntry* sel = SelectedDrawEntry();
		if (!sel)
		{
			ImGui::TextWrapped("Select a log to see its squad players.");
			return;
		}

		ImGui::TextUnformatted(sel->encounter.empty() ? sel->fileName.c_str() : sel->encounter.c_str());
		ImGui::TextColored(HelperTheme::Muted,
			"%d players in this run", static_cast<int>(sel->players.size()));

		if (sel->players.empty())
		{
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f),
				"No player data - Parse or Load DPS/boons for this log.");
			return;
		}

		if (ImGui::BeginTable("###gw2igh_lm_paggs", 6,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
					ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
				ImVec2(-FLT_MIN, -FLT_MIN)))
		{
			ImGui::TableSetupColumn("Account", ImGuiTableColumnFlags_WidthStretch, 1.4f);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.2f);
			ImGui::TableSetupColumn("Prof", ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableSetupColumn("DPS", ImGuiTableColumnFlags_WidthStretch, 0.7f);
			ImGui::TableSetupColumn("Guild", ImGuiTableColumnFlags_WidthStretch, 0.8f);
			ImGui::TableSetupColumn("G", ImGuiTableColumnFlags_WidthStretch, 0.35f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();
			for (const PlayerInfo& p : sel->players)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.account.empty() ? "-" : p.account.c_str());
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
				const std::string g = GuildLabelFor(p);
				ImGui::TextUnformatted(g.empty() ? "-" : g.c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%d", p.group);
			}
			ImGui::EndTable();
		}
	}

} // namespace LogManagerDetail
