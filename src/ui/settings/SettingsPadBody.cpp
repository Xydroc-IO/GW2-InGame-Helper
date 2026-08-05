#include "SettingsPad.h"

#include "UI_Browse.h"

#include "AddonPaths.h"
#include "AspectLayout.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "LivePanels.h"
#include "Settings.h"
#include "Sites.h"
#include "UiScale.h"

#include "imgui/imgui.h"

#include <windows.h>
#include <shellapi.h>

void SettingsPad::DrawContents()
{
	size_t count = 0;
	Sites::All(&count);
	if (count > 0)
	{
		ImGui::TextUnformatted("Default landing site");
		UI_Browse_DrawDefaultSitePicker();
		ImGui::TextColored(HelperTheme::Muted, "Home button uses this. Also used when no tabs are saved yet.");
	}

	if (ImGui::SliderFloat("Opacity###gw2igh_opacity", &G::Opacity, 0.15f, 1.f, "%.2f"))
		Settings::SetDirty();
	if (ImGui::SliderFloat("Font scale###gw2igh_font", &G::FontScale, 0.75f, 2.f, "%.2f"))
	{
		G::FontScaleAuto = false;
		Settings::SetDirty();
	}
	ImGui::TextColored(HelperTheme::Muted, "Default 1.00×. Content also scales with each panel’s size as you resize.");
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
		ImGui::TextColored(HelperTheme::Muted,
			"Auto: height + %s aspect (cap ~1.35×). First-open window uses 16:9 / 21:9 / 32:9 layouts.",
			aspect);
	}
	if (ImGui::Checkbox("Keep browser warm when closed###gw2igh_warm", &G::KeepHelperWarm))
		Settings::SetDirty();
	ImGui::TextColored(HelperTheme::Muted, "Faster reopen; uses more RAM while the helper is hidden.");

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextUnformatted("GW2 API key (Live panels)");
	ImGui::TextColored(HelperTheme::Muted,
		"Read-only key from account.arena.net. Scopes: account + progression (Vault); "
		"wallet (Wallet pad); inventories + unlocks + characters (mats / Legendaries); "
		"tradingpost (TP delivery box); progression (Vault + event claim marks). "
		"Stored only in this addon’s settings.ini — never shared.");
	ImGui::SetNextItemWidth(-1.f);
	if (ImGui::InputTextWithHint("###gw2igh_apikey", "Paste API key here…", G::Gw2ApiKey, sizeof(G::Gw2ApiKey),
			ImGuiInputTextFlags_Password | ImGuiInputTextFlags_AutoSelectAll))
	{
		Settings::SetDirty();
		LivePanels::InvalidateCaches(AddonPaths::DataDir());
	}
	if (G::Gw2ApiKey[0])
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "Key saved — Reload Live tabs to refresh.");
	else
		ImGui::TextColored(HelperTheme::Muted, "No key — Vault/Progress show public data until you add one.");
	if (ImGui::Button("Clear API key###gw2igh_apikey_clear"))
	{
		G::Gw2ApiKey[0] = 0;
		Settings::SetDirty();
		LivePanels::InvalidateCaches(AddonPaths::DataDir());
	}
	ImGui::SameLine();
	if (ImGui::Button("Create key on account.arena.net###gw2igh_apikey_web"))
		ShellExecuteA(nullptr, "open", "https://account.arena.net/applications", nullptr, nullptr, SW_SHOWNORMAL);

	ImGui::Spacing();
	ImGui::TextWrapped(
		"Hotkeys stay in Nexus → Options → Keybinds (Nexus owns rebinding). "
		"Defaults: Ctrl+Shift+H helper | A Account | G Pathing | E Events | N Notes | F marker interact. "
		"In helper: Ctrl+T new tab | Ctrl+W close | Ctrl+Tab cycle | Ctrl+F find.");
	Settings::Save(false);
}
