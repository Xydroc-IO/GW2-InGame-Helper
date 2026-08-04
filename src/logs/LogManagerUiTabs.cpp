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
