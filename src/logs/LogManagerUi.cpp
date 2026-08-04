#include "LogManagerShared.h"
#include "LogManagerUiInternal.h"
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
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "dps.report %d / %d…",
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
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"POST EVTC to https://dps.report/uploadContent (max 48 MB). "
				"Permalink is saved on the log; open it from the detail pane.");
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

} // namespace LogManagerDetail
