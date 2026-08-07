#include "SettingsPad.h"

#include "UI_Browse.h"

#include "AddonPaths.h"
#include "AspectLayout.h"
#include "Globals.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "LivePanels.h"
#include "PadNav.h"
#include "PanelBinds.h"
#include "Settings.h"
#include "Sites.h"
#include "UiScale.h"

#include "imgui/imgui.h"

#include <windows.h>
#include <shellapi.h>

namespace
{
	void MutedWrap(const char* text)
	{
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted, "%s", text);
		PadNav::PopWrap();
	}

	void DrawGeneralTab()
	{
		size_t count = 0;
		Sites::All(&count);
		if (count > 0)
		{
			PadNav::Meta("Default landing site");
			UI_Browse_DrawDefaultSitePicker();
			MutedWrap("Home button uses this. Also used when no tabs are saved yet.");
		}

		PadNav::PushLabeledItemWidth();
		if (ImGui::SliderFloat("Opacity###gw2igh_opacity", &G::Opacity, 0.15f, 1.f, "%.2f"))
			Settings::SetDirty();
		if (ImGui::SliderFloat("Font scale###gw2igh_font", &G::FontScale, 0.75f, 2.f, "%.2f"))
		{
			G::FontScaleAuto = false;
			Settings::SetDirty();
		}
		PadNav::PopLabeledItemWidth();
		MutedWrap("Default 1.00x — applies the same to every panel.");
		if (ImGui::Checkbox("Auto font scale###gw2igh_font_auto", &G::FontScaleAuto))
		{
			if (G::FontScaleAuto)
			{
				const ImGuiIO& io = ImGui::GetIO();
				if (io.DisplaySize.x > 100.f && io.DisplaySize.y > 100.f)
					G::FontScale = UiScale::Suggest(io.DisplaySize.x, io.DisplaySize.y);
			}
			Settings::SetDirty();
		}
		if (G::FontScaleAuto)
		{
			const ImGuiIO& io = ImGui::GetIO();
			const char* aspect = "16:9";
			if (io.DisplaySize.x > 100.f && io.DisplaySize.y > 100.f)
				aspect = AspectLayout::ClassLabel(
					AspectLayout::Classify(io.DisplaySize.x, io.DisplaySize.y));
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted,
				"Auto: display height + %s (cap ~1.15x).",
				aspect);
			PadNav::PopWrap();
		}
		if (ImGui::Checkbox("Keep browser warm when closed###gw2igh_warm", &G::KeepHelperWarm))
			Settings::SetDirty();
		MutedWrap("Faster reopen; uses more RAM while the helper is hidden.");
		if (ImGui::Checkbox("Show side rail labels###gw2igh_rail_labels", &G::ShowRailLabels))
			Settings::SetDirty();
		MutedWrap("Off = compact game-like icon dock (hover for names). On = icon + text.");

		ImGui::Separator();
		PadNav::Meta("GW2 API key (Live panels)");
		MutedWrap(
			"Read-only key from account.arena.net (account, progression, wallet, "
			"inventories, unlocks, tradingpost as needed). Stored only in this addon's settings.ini.");
		ImGui::SetNextItemWidth(-1.f);
		if (ImGui::InputTextWithHint("###gw2igh_apikey", "Paste API key here...", G::Gw2ApiKey, sizeof(G::Gw2ApiKey),
				ImGuiInputTextFlags_Password | ImGuiInputTextFlags_AutoSelectAll))
		{
			Settings::SetDirty();
			LivePanels::InvalidateCaches(AddonPaths::DataDir());
		}
		if (G::Gw2ApiKey[0])
			ImGui::TextColored(HelperTheme::Ok, "Key saved - Reload Live tabs to refresh.");
		else
			MutedWrap("No key - Vault/Progress show public data until you add one.");
		if (ImGui::Button("Clear API key###gw2igh_apikey_clear"))
		{
			G::Gw2ApiKey[0] = 0;
			Settings::SetDirty();
			LivePanels::InvalidateCaches(AddonPaths::DataDir());
		}
		PadNav::WrapSameLine(PadNav::ButtonWidth("Create key on account.arena.net"));
		if (ImGui::Button("Create key on account.arena.net###gw2igh_apikey_web"))
			ShellExecuteA(nullptr, "open", "https://account.arena.net/applications", nullptr, nullptr, SW_SHOWNORMAL);

		PadNav::PushWrap();
		ImGui::TextWrapped(
			"Helper open: Ctrl+Shift+H (Nexus QuickAccess). "
			"Panel chords: Keybinds tab (not Nexus Options). "
			"In helper: Ctrl+T new tab | Ctrl+W close | Ctrl+Tab cycle | Ctrl+F find.");
		PadNav::PopWrap();
	}
}

void SettingsPad::DrawContents()
{
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 3.f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 3.f));

	static int sTab = 0;
	static const char* kTabs[] = { "General", "Keybinds" };
	static const int kIcons[] = {
		static_cast<int>(Gw2Ui::Icon::Options),
		static_cast<int>(Gw2Ui::Icon::Bag),
	};
	sTab = PadNav::DrawSideRail("###gw2igh_settings_nav", kTabs, 2, sTab, 0.f, kIcons);

	ImGui::BeginChild("###gw2igh_settings_body", ImVec2(0.f, 0.f), true);
	if (sTab == 0)
		DrawGeneralTab();
	else
		PanelBinds::DrawSettingsTab();
	ImGui::EndChild();
	ImGui::PopStyleVar(2);
	Settings::Save(false);
}
