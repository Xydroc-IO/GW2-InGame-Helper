#include "LogManagerShared.h"

#include "LogManagerUpload.h"
#include "LogManagerEi.h"

#include "EiRuntime.h"
#include "Globals.h"
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
	void DrawBusyOrStatus()
	{
		if (gEiInstallBusy.load())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "%s",
				gEiStatus[0] ? gEiStatus : "Installing Elite Insights…");
		else if (gScanBusy.load())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Scanning…");
		else if (gParseBusy.load())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Parsing %d / %d…",
				gParseDone.load(), gParseTotal.load());
		else if (gUploadBusy.load())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Uploading %d / %d…",
				gUploadDone.load(), gUploadTotal.load());
		else if (gHydrateBusy.load())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "%s",
				gStatus[0] ? gStatus : "Loading report metadata…");
		else if (gStatus[0])
			ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", gStatus);
	}

	void DrawToolbar(const std::vector<const LogEntry*>& filtered, bool hasDotNet)
	{
		if (ImGui::Button("Rescan###gw2igh_lm_scan"))
			BeginScan();
		ImGui::SameLine(0.f, 4.f);

		const bool canParse = hasDotNet && !gParseBusy.load() && !gScanBusy.load() &&
			!gEiInstallBusy.load() && G::EliteInsightsPath[0] && PathExistsUtf8(G::EliteInsightsPath);
		if (canParse)
		{
			if (ImGui::Button("Parse###gw2igh_lm_parse"))
				BeginParsePending();
		}
		else
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
			ImGui::Button("Parse###gw2igh_lm_parse");
			ImGui::PopStyleVar();
		}
		ImGui::SameLine(0.f, 4.f);

		if (ImGui::Button("Upload filtered###gw2igh_lm_upall"))
		{
			std::vector<std::string> paths;
			paths.reserve(filtered.size());
			for (const LogEntry* e : filtered)
				paths.push_back(e->pathUtf8);
			BeginUpload(paths);
		}
		ImGui::SameLine(0.f, 4.f);

		if (gHydrateBusy.load())
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
			ImGui::Button("Load DPS/boons###gw2igh_lm_hydrate");
			ImGui::PopStyleVar();
		}
		else if (ImGui::Button("Load DPS/boons###gw2igh_lm_hydrate"))
			BeginHydrateFromReports();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Fill encounter, DPS, and boon uptimes from dps.report (getJson). No re-upload.");
		ImGui::SameLine(0.f, 4.f);

		if (ImGui::Button("Open folder###gw2igh_lm_openfolder"))
		{
			const std::wstring w = Utf8ToWide(G::LogFolder);
			if (!w.empty())
				ShellExecuteW(nullptr, L"explore", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}

		if (!hasDotNet)
		{
			ImGui::SameLine(0.f, 8.f);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.35f, 0.12f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.42f, 0.15f, 1.f));
			if (ImGui::Button("Needs .NET 8###gw2igh_lm_neednet"))
				gFocusSetupTab = true;
			ImGui::PopStyleColor(2);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Elite Insights needs .NET 8 Desktop Runtime — open Setup.");
		}

		ImGui::SameLine(0.f, 10.f);
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f), "%d shown / %d",
			static_cast<int>(filtered.size()), static_cast<int>(gDraw.size()));
		ImGui::SameLine(0.f, 10.f);
		DrawBusyOrStatus();
	}

	void DrawFilterPane()
	{
		ImGui::TextUnformatted("Filters");
		ImGui::Separator();
		ImGui::SetNextItemWidth(-1.f);
		ImGui::InputTextWithHint("###gw2igh_lm_search", "Search file or encounter…",
			gSearch, sizeof(gSearch));
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "Result");
		ImGui::RadioButton("All###gw2igh_lm_res0", &gResultFilter, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Kills###gw2igh_lm_res1", &gResultFilter, 1);
		ImGui::RadioButton("Fails###gw2igh_lm_res2", &gResultFilter, 2);
		ImGui::SameLine();
		ImGui::RadioButton("Unknown###gw2igh_lm_res3", &gResultFilter, 3);
		ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "Mode");
		ImGui::RadioButton("All###gw2igh_lm_mode0", &gModeFilter, 0);
		ImGui::SameLine();
		ImGui::RadioButton("Normal###gw2igh_lm_mode1", &gModeFilter, 1);
		ImGui::RadioButton("CM###gw2igh_lm_mode2", &gModeFilter, 2);
		ImGui::SameLine();
		ImGui::RadioButton("LCM###gw2igh_lm_mode3", &gModeFilter, 3);
		ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "Time");
		ImGui::RadioButton("All###gw2igh_lm_day0", &gDaysCombo, 0);
		ImGui::RadioButton("1 day###gw2igh_lm_day1", &gDaysCombo, 1);
		ImGui::SameLine();
		ImGui::RadioButton("3 days###gw2igh_lm_day2", &gDaysCombo, 2);
		ImGui::RadioButton("7 days###gw2igh_lm_day3", &gDaysCombo, 3);
		ImGui::SameLine();
		ImGui::RadioButton("30 days###gw2igh_lm_day4", &gDaysCombo, 4);
		ImGui::Spacing();
		if (ImGui::Checkbox("Group by encounter###gw2igh_lm_groupby", &G::LogManagerGroupByEncounter))
		{
			Settings::SetDirty();
			if (G::LogManagerGroupByEncounter)
				gExpandGroupsOnce = true;
		}
		if (ImGui::Checkbox("Auto-parse after scan###gw2igh_lm_autoparse", &G::LogManagerAutoParse))
			Settings::SetDirty();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("After Rescan / open, parse pending logs with Elite Insights automatically.");
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			G::LogManagerGroupByEncounter
				? "Collapsible sections · newest encounter first"
				: "Flat list · use Search to narrow");
		ImGui::PopTextWrapPos();
		ImGui::Spacing();
		if (ImGui::SmallButton("Clear filters###gw2igh_lm_clearf"))
		{
			gSearch[0] = 0;
			gResultFilter = static_cast<int>(ResultFilter::All);
			gModeFilter = static_cast<int>(ModeFilter::All);
			gDaysCombo = 0;
		}
	}

	void SelectLogByPath(const std::string& pathUtf8)
	{
		for (int j = 0; j < static_cast<int>(gDraw.size()); ++j)
		{
			if (gDraw[static_cast<size_t>(j)].pathUtf8 == pathUtf8)
			{
				gSelected = j;
				break;
			}
		}
	}

	void DrawLogEntryRow(const LogEntry* e, bool showEncounter)
	{
		if (!e)
			return;
		ImGui::PushID(e->pathUtf8.c_str());
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		const bool sel = (gSelected >= 0 && gSelected < static_cast<int>(gDraw.size()) &&
			gDraw[static_cast<size_t>(gSelected)].pathUtf8 == e->pathUtf8);
		char label[48];
		std::snprintf(label, sizeof(label), "%s", FmtTime(e->encounterTime).c_str());
		if (ImGui::Selectable(label, sel, ImGuiSelectableFlags_SpanAllColumns))
			SelectLogByPath(e->pathUtf8);
		if (showEncounter)
		{
			ImGui::TableNextColumn();
			if (!e->encounter.empty())
				ImGui::TextUnformatted(e->encounter.c_str());
			else
				ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.f), "%s", e->fileName.c_str());
		}
		ImGui::TableNextColumn();
		if (e->result == 1)
			ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.50f, 1.f), "Kill");
		else if (e->result == 0)
			ImGui::TextColored(ImVec4(0.90f, 0.45f, 0.40f, 1.f), "Fail");
		else if (e->state == ParseState::Pending)
			ImGui::TextColored(ImVec4(0.60f, 0.60f, 0.40f, 1.f), "…");
		else
			ImGui::TextUnformatted(ResultLabel(e->result));
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(e->mode.empty() ? "-" : e->mode.c_str());
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(FmtDuration(e->durationMs).c_str());
		ImGui::TableNextColumn();
		if (e->compDps > 0)
			ImGui::Text("%d", e->compDps);
		else
			ImGui::TextUnformatted("-");
		ImGui::TableNextColumn();
		ImGui::Text("%d", static_cast<int>(e->players.size()));
		ImGui::PopID();
	}

	time_t LogSortTime(const LogEntry* e)
	{
		if (!e)
			return 0;
		if (e->encounterTime > 0)
			return e->encounterTime;
		return FileTimeToUnix(e->mtime);
	}

	void DrawLogTableGrouped(const std::vector<const LogEntry*>& filtered)
	{
		struct EncGroup
		{
			std::string key;
			std::string label;
			std::vector<const LogEntry*> logs;
			time_t lastTime = 0;
			long long bestKillMs = 0;
			int kills = 0;
		};

		std::unordered_map<std::string, EncGroup> map;
		map.reserve(filtered.size());
		for (const LogEntry* e : filtered)
		{
			if (!e)
				continue;
			std::string key = e->encounter;
			std::string label = e->encounter;
			if (key.empty())
			{
				key = "\x01unknown";
				label = "Unknown encounter";
			}
			EncGroup& g = map[key];
			if (g.key.empty())
			{
				g.key = key;
				g.label = label;
			}
			g.logs.push_back(e);
			const time_t t = LogSortTime(e);
			if (t > g.lastTime)
				g.lastTime = t;
			if (e->result == 1)
			{
				g.kills += 1;
				if (e->durationMs > 0 && (g.bestKillMs <= 0 || e->durationMs < g.bestKillMs))
					g.bestKillMs = e->durationMs;
			}
		}

		std::vector<EncGroup*> groups;
		groups.reserve(map.size());
		for (auto& kv : map)
			groups.push_back(&kv.second);
		std::sort(groups.begin(), groups.end(), [](const EncGroup* a, const EncGroup* b) {
			if (a->lastTime != b->lastTime)
				return a->lastTime > b->lastTime;
			return a->label < b->label;
		});

		ImGui::BeginChild("###gw2igh_lm_groupscroll", ImVec2(-FLT_MIN, -FLT_MIN), false);
		const bool forceOpen = gExpandGroupsOnce;
		for (EncGroup* g : groups)
		{
			std::sort(g->logs.begin(), g->logs.end(), [](const LogEntry* a, const LogEntry* b) {
				return LogSortTime(a) > LogSortTime(b);
			});

			const size_t idHash = std::hash<std::string>{}(g->key);
			char header[288];
			if (g->bestKillMs > 0)
			{
				std::snprintf(header, sizeof(header),
					"%s  (%d)  ·  %d kill%s  ·  best %s  ·  last %s###gw2igh_enc_%zu",
					g->label.c_str(),
					static_cast<int>(g->logs.size()),
					g->kills, g->kills == 1 ? "" : "s",
					FmtDuration(g->bestKillMs).c_str(),
					FmtTime(g->lastTime).c_str(),
					idHash);
			}
			else
			{
				std::snprintf(header, sizeof(header),
					"%s  (%d)  ·  last %s###gw2igh_enc_%zu",
					g->label.c_str(),
					static_cast<int>(g->logs.size()),
					FmtTime(g->lastTime).c_str(),
					idHash);
			}

			if (forceOpen)
				ImGui::SetNextItemOpen(true, ImGuiCond_Always);
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.17f, 0.14f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.24f, 0.22f, 0.16f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.28f, 0.26f, 0.18f, 1.f));
			const bool open = ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen);
			ImGui::PopStyleColor(3);
			if (!open)
				continue;

			/* Unique table id per encounter — shared ids fight column layout across groups. */
			char tableId[64];
			std::snprintf(tableId, sizeof(tableId), "###gw2igh_lm_gtable_%zu", idHash);
			if (ImGui::BeginTable(tableId, 6,
					ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
						ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
					ImVec2(-FLT_MIN, 0.f)))
			{
				ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch, 0.34f);
				ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthStretch, 0.12f);
				ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthStretch, 0.10f);
				ImGui::TableSetupColumn("Dur", ImGuiTableColumnFlags_WidthStretch, 0.12f);
				ImGui::TableSetupColumn("Squad", ImGuiTableColumnFlags_WidthStretch, 0.16f);
				ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthStretch, 0.06f);
				ImGui::TableHeadersRow();
				for (const LogEntry* e : g->logs)
					DrawLogEntryRow(e, false);
				ImGui::EndTable();
			}
		}
		gExpandGroupsOnce = false;
		ImGui::EndChild();
	}

	void DrawLogTable(const std::vector<const LogEntry*>& filtered)
	{
		if (filtered.empty())
		{
			ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.f), "No logs match filters.");
			return;
		}

		if (G::LogManagerGroupByEncounter)
		{
			DrawLogTableGrouped(filtered);
			return;
		}

		const ImVec2 tableSize(-FLT_MIN, -FLT_MIN);
		if (ImGui::BeginTable("###gw2igh_lm_table", 7,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
					ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
				tableSize))
		{
			ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch, 0.22f);
			ImGui::TableSetupColumn("Encounter", ImGuiTableColumnFlags_WidthStretch, 0.36f);
			ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthStretch, 0.10f);
			ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthStretch, 0.08f);
			ImGui::TableSetupColumn("Dur", ImGuiTableColumnFlags_WidthStretch, 0.10f);
			ImGui::TableSetupColumn("Squad", ImGuiTableColumnFlags_WidthStretch, 0.10f);
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthStretch, 0.04f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			for (const LogEntry* e : filtered)
				DrawLogEntryRow(e, true);
			ImGui::EndTable();
		}
	}

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
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f), "%s", sel->fileName.c_str());
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
			ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.58f, 1.f),
				"No player data — Parse, Upload, or Load DPS/boons.");
		else if (!PlayersHaveDps(sel->players) && !PlayersHaveBoons(sel->players))
		{
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f),
				"Names loaded — click Load DPS/boons (or Parse with EI).");
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
				return p.guildId.substr(0, 8) + "…";
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
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			"%d players in this run", static_cast<int>(sel->players.size()));

		if (sel->players.empty())
		{
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f),
				"No player data — Parse or Load DPS/boons for this log.");
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

	void DrawKillProofTab()
	{
		const LogEntry* sel = SelectedDrawEntry();
		if (!sel)
		{
			ImGui::TextWrapped("Select a log to look up KillProof for its squad.");
			return;
		}

		ImGui::TextUnformatted(sel->encounter.empty() ? sel->fileName.c_str() : sel->encounter.c_str());
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			"Public killproof.me profiles for this run");

		if (sel->players.empty())
		{
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f),
				"No player data — Parse or Load DPS/boons first so account names exist.");
			return;
		}

		int withAccount = 0, kpOk = 0, kpMissing = 0, kpPending = 0;
		for (const PlayerInfo& p : sel->players)
		{
			if (p.account.empty())
				continue;
			++withAccount;
			if (p.kpState == 2) ++kpOk;
			else if (p.kpState == 3 || p.kpState == 4) ++kpMissing;
			else ++kpPending;
		}

		if (gKillProofBusy.load())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "%s",
				gStatus[0] ? gStatus : "Loading killproof.me…");
		else if (withAccount == 0)
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f),
				"No account names — Load DPS/boons for full EI JSON.");
		else if (kpOk > 0 || kpMissing > 0)
			ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f),
				"%d loaded · %d none/private · %d pending",
				kpOk, kpMissing, kpPending);
		else
			ImGui::TextColored(ImVec4(0.75f, 0.70f, 0.45f, 1.f),
				"Click Load to fetch LI / LD / tokens from killproof.me.");

		const bool busy = gKillProofBusy.load();
		if (busy)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
			ImGui::Button("Loading…###gw2igh_lm_loadkp");
			ImGui::PopStyleVar();
		}
		else if (ImGui::Button("Load KillProof###gw2igh_lm_loadkp"))
			KickKillProofForSelected(sel, true);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip(
				"Fetch Legendary Insights, Divinations, UFE, and encounter tokens.\n"
				"Only public killproof.me profiles are available.");
		ImGui::SameLine();
		if (ImGui::SmallButton("killproof.me###gw2igh_lm_kpweb"))
			ShellExecuteA(nullptr, "open", "https://killproof.me/", nullptr, nullptr, SW_SHOWNORMAL);

		/* Auto-fill once when this tab is open and KP not loaded yet. */
		if (!busy && withAccount > 0)
		{
			bool need = false;
			for (const PlayerInfo& p : sel->players)
			{
				if (!p.account.empty() && p.kpState == 0)
				{
					need = true;
					break;
				}
			}
			if (need)
				KickKillProofForSelected(sel, false);
		}

		const char* kpCol = "Token";
		int bossId = 0;
		const char* bossLabel = nullptr;
		if (BossTokenForEncounter(sel->encounter, bossId, bossLabel) && bossLabel)
			kpCol = bossLabel;

		if (ImGui::BeginTable("###gw2igh_lm_kptab", 8,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
					ImGuiTableFlags_ScrollX | ImGuiTableFlags_Resizable |
					ImGuiTableFlags_SizingStretchProp,
				ImVec2(-FLT_MIN, -FLT_MIN)))
		{
			ImGui::TableSetupColumn("Account", ImGuiTableColumnFlags_WidthStretch, 1.5f);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.1f);
			ImGui::TableSetupColumn("LI", ImGuiTableColumnFlags_WidthFixed, 44.f);
			ImGui::TableSetupColumn("LD", ImGuiTableColumnFlags_WidthFixed, 44.f);
			ImGui::TableSetupColumn("UFE", ImGuiTableColumnFlags_WidthFixed, 52.f);
			ImGui::TableSetupColumn(kpCol, ImGuiTableColumnFlags_WidthFixed, 48.f);
			ImGui::TableSetupColumn("Prof", ImGuiTableColumnFlags_WidthStretch, 0.9f);
			ImGui::TableSetupColumn("G", ImGuiTableColumnFlags_WidthFixed, 24.f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			auto kpCell = [](int v, int state) {
				if (state == 1)
				{
					ImGui::TextColored(ImVec4(0.70f, 0.68f, 0.45f, 1.f), "…");
					return;
				}
				if (state == 3)
				{
					ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.48f, 1.f), "—");
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("No public killproof.me profile");
					return;
				}
				if (state == 4)
				{
					ImGui::TextColored(ImVec4(0.90f, 0.50f, 0.40f, 1.f), "!");
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("killproof.me request failed — try Load again");
					return;
				}
				if (state == 0 || v < 0)
				{
					ImGui::TextUnformatted("—");
					return;
				}
				ImGui::Text("%d", v);
			};

			std::vector<const PlayerInfo*> rows;
			rows.reserve(sel->players.size());
			for (const PlayerInfo& p : sel->players)
				rows.push_back(&p);
			std::sort(rows.begin(), rows.end(), [](const PlayerInfo* a, const PlayerInfo* b) {
				if (a->kpLi != b->kpLi)
					return a->kpLi > b->kpLi;
				return a->account < b->account;
			});

			for (const PlayerInfo* pp : rows)
			{
				const PlayerInfo& p = *pp;
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				if (!p.kpUrl.empty() && !p.account.empty())
				{
					if (ImGui::Selectable(p.account.c_str(), false, ImGuiSelectableFlags_None))
						ShellExecuteA(nullptr, "open", p.kpUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Open killproof.me profile");
				}
				else
					ImGui::TextUnformatted(p.account.empty() ? "-" : p.account.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.name.empty() ? "-" : p.name.c_str());
				ImGui::TableNextColumn();
				kpCell(p.kpLi, p.kpState);
				ImGui::TableNextColumn();
				kpCell(p.kpLd, p.kpState);
				ImGui::TableNextColumn();
				kpCell(p.kpUfe, p.kpState);
				ImGui::TableNextColumn();
				if (bossId > 0)
					kpCell(p.kpBoss, p.kpState);
				else
					ImGui::TextUnformatted("—");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.profession.empty() ? "-" : p.profession.c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%d", p.group);
			}
			ImGui::EndTable();
		}
	}

	void DrawGuildsTab(const std::vector<const LogEntry*>& /*filtered*/)
	{
		const LogEntry* sel = SelectedDrawEntry();
		if (!sel)
		{
			ImGui::TextWrapped("Select a log to see guilds in that run.");
			return;
		}

		ImGui::TextUnformatted(sel->encounter.empty() ? sel->fileName.c_str() : sel->encounter.c_str());

		struct Acc
		{
			std::string label;
			int players = 0;
			int dpsSum = 0;
		};
		std::unordered_map<std::string, Acc> map;
		for (const PlayerInfo& p : sel->players)
		{
			std::string key = p.guildTag;
			std::string label = p.guildTag;
			if (key.empty() && !p.guildId.empty())
			{
				key = p.guildId;
				label = p.guildId.size() > 8 ? p.guildId.substr(0, 8) + "…" : p.guildId;
			}
			if (key.empty())
				continue;
			Acc& a = map[key];
			a.label = label;
			a.players += 1;
			a.dpsSum += p.dps > 0 ? p.dps : 0;
		}

		std::vector<std::pair<std::string, Acc>> rows;
		rows.reserve(map.size());
		for (auto& kv : map)
			rows.emplace_back(kv.first, std::move(kv.second));
		std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
			if (a.second.players != b.second.players)
				return a.second.players > b.second.players;
			return a.second.label < b.second.label;
		});

		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			"%d guilds in this run", static_cast<int>(rows.size()));

		if (rows.empty())
		{
			ImGui::TextWrapped(
				"No guilds on this squad after load. Click Load DPS/boons to refresh; "
				"players without a guild show as empty.");
			return;
		}

		if (ImGui::BeginTable("###gw2igh_lm_gaggs", 3,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
					ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
				ImVec2(-FLT_MIN, -FLT_MIN)))
		{
			ImGui::TableSetupColumn("Guild", ImGuiTableColumnFlags_WidthStretch, 2.0f);
			ImGui::TableSetupColumn("Players", ImGuiTableColumnFlags_WidthStretch, 0.7f);
			ImGui::TableSetupColumn("DPS sum", ImGuiTableColumnFlags_WidthStretch, 0.9f);
			ImGui::TableHeadersRow();
			for (const auto& row : rows)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(row.second.label.c_str());
				ImGui::TableNextColumn();
				ImGui::Text("%d", row.second.players);
				ImGui::TableNextColumn();
				if (row.second.dpsSum > 0)
					ImGui::Text("%d", row.second.dpsSum);
				else
					ImGui::TextUnformatted("-");
			}
			ImGui::EndTable();
		}
	}

	void DrawFastestTab(const std::vector<const LogEntry*>& filtered)
	{
		std::vector<FastestKill> kills;
		BuildFastest(filtered, kills);
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			"Best kill time per encounter (filtered)");
		if (ImGui::BeginTable("###gw2igh_lm_fast", 3,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
		{
			ImGui::TableSetupColumn("Encounter");
			ImGui::TableSetupColumn("Time");
			ImGui::TableSetupColumn("File");
			ImGui::TableHeadersRow();
			for (const FastestKill& k : kills)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(k.encounter.c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(FmtDuration(k.durationMs).c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(k.fileName.c_str());
			}
			ImGui::EndTable();
		}
	}

	void DrawSetupTab(bool hasDotNet)
	{
		ImGui::TextWrapped(
			"Browse ArcDPS logs. Elite Insights auto-updates from GitHub latest (MIT, baaron4).");
		ImGui::Separator();

		if (!hasDotNet)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.72f, 0.35f, 1.f));
			ImGui::TextWrapped(
				".NET 8 Desktop Runtime not detected — Elite Insights cannot parse until it is installed.");
			ImGui::PopStyleColor();
			if (EiRuntime::IsWine())
			{
				ImGui::TextColored(ImVec4(0.75f, 0.70f, 0.45f, 1.f), "%s",
					"Proton/Wine: install into this game's Windows prefix — Linux distro packages will not work.");
			}
			if (ImGui::Button("Install .NET 8 Runtime###gw2igh_lm_dotnet_install"))
				EiRuntime::OpenDotNet8Installer();
			ImGui::SameLine();
			if (ImGui::Button("Recheck .NET###gw2igh_lm_dotnet_recheck"))
			{
				EiRuntime::InvalidateDotNet8Cache();
				std::snprintf(gStatus, sizeof(gStatus),
					EiRuntime::HasDotNet8Runtime()
						? ".NET 8 runtime found."
						: ".NET 8 still not detected.");
			}
			ImGui::Separator();
		}
		else
			ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), ".NET 8 Desktop Runtime detected.");

		ImGui::TextUnformatted("Log folder");
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputTextWithHint("###gw2igh_lm_folder", "arcdps.cbtlogs path",
				G::LogFolder, sizeof(G::LogFolder)))
			Settings::SetDirty();

		ImGui::TextUnformatted("Elite Insights CLI");
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputTextWithHint("###gw2igh_lm_ei",
				"Auto-filled after install, or custom path",
				G::EliteInsightsPath, sizeof(G::EliteInsightsPath)))
			Settings::SetDirty();

		ImGui::TextUnformatted("dps.report token (optional)");
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputTextWithHint("###gw2igh_lm_token", "User token",
				G::DpsReportToken, sizeof(G::DpsReportToken),
				ImGuiInputTextFlags_Password))
			Settings::SetDirty();

		ImGui::Spacing();
		if (gEiInstallBusy.load())
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
			ImGui::Button("Install / Update EI###gw2igh_lm_eiinst");
			ImGui::PopStyleVar();
		}
		else if (ImGui::Button("Install / Update EI###gw2igh_lm_eiinst"))
			BeginEiEnsure(true);
		ImGui::SameLine();
		if (ImGui::Button("EI releases###gw2igh_lm_eihelp"))
			ShellExecuteA(nullptr, "open",
				"https://github.com/baaron4/GW2-Elite-Insights-Parser/releases",
				nullptr, nullptr, SW_SHOWNORMAL);
		ImGui::SameLine();
		if (ImGui::Button("Open log folder###gw2igh_lm_setup_folder"))
		{
			const std::wstring w = Utf8ToWide(G::LogFolder);
			if (!w.empty())
				ShellExecuteW(nullptr, L"explore", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}

		if (gEiInstallBusy.load())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "%s",
				gEiStatus[0] ? gEiStatus : "Installing Elite Insights…");
	}

} // namespace LogManagerDetail
