#include "SettingsPad.h"

#include "UI_Browse.h"

#include "AddonPaths.h"
#include "AspectLayout.h"
#include "BrowserTabs.h"
#include "CheatSheets.h"
#include "Globals.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "HomePage.h"
#include "LivePanels.h"
#include "PadNav.h"
#include "PanelBinds.h"
#include "Settings.h"
#include "Sites.h"
#include "UiScale.h"
#include "UserTheme.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <string>
#include <vector>

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

		{
			/* Cache list — do not scan the filesystem every ImGui frame. */
			static bool sSeeded = false;
			static std::vector<std::string> sThemes;
			static DWORD sLastHeavyMs = 0;
			if (!sSeeded)
			{
				UserTheme::EnsureSeed();
				sThemes = UserTheme::ListThemes();
				sSeeded = true;
			}

			int cur = 0; /* 0 = Default */
			for (size_t i = 0; i < sThemes.size(); ++i)
			{
				if (G::ThemeId[0] && sThemes[i] == G::ThemeId)
				{
					cur = static_cast<int>(i) + 1;
					break;
				}
			}
			const char* preview = (cur == 0) ? "Default"
				: sThemes[static_cast<size_t>(cur - 1)].c_str();
			PadNav::PushLabeledItemWidth();
			if (ImGui::BeginCombo("Theme###gw2igh_theme", preview))
			{
				if (ImGui::Selectable("Default", cur == 0))
				{
					G::ThemeId[0] = 0;
					UserTheme::Apply("default");
					Settings::SetDirty();
				}
				for (size_t i = 0; i < sThemes.size(); ++i)
				{
					const bool sel = (cur == static_cast<int>(i) + 1);
					if (ImGui::Selectable(sThemes[i].c_str(), sel))
					{
						std::snprintf(G::ThemeId, sizeof(G::ThemeId), "%s", sThemes[i].c_str());
						UserTheme::Apply(G::ThemeId);
						Settings::SetDirty();
					}
				}
				ImGui::EndCombo();
			}
			PadNav::PopLabeledItemWidth();
			if (ImGui::SmallButton("Open themes folder###gw2igh_theme_open"))
			{
				const DWORD now = GetTickCount();
				if (now - sLastHeavyMs >= 750u)
				{
					sLastHeavyMs = now;
					const std::wstring dir = AddonPaths::ThemesDir();
					if (!dir.empty())
						ShellExecuteW(nullptr, L"explore", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
				}
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Reload themes###gw2igh_theme_reload"))
			{
				const DWORD now = GetTickCount();
				if (now - sLastHeavyMs >= 750u)
				{
					sLastHeavyMs = now;
					UserTheme::EnsureSeed();
					sThemes = UserTheme::ListThemes();
					UserTheme::Reload();
					/* Light page CSS refresh only — never extract packs from Settings. */
					(void)HomePage::EnsureFileUrl(AddonPaths::DataDir());
					CheatSheets::RefreshUserThemeCss();
					Settings::SetDirty();
				}
			}
			MutedWrap(
				"Drop a folder with theme.ini under config/themes/. "
				"Pads update immediately; reopen helper pages to refresh colors.");
		}

		PadNav::PushLabeledItemWidth();
		if (ImGui::SliderFloat("Opacity###gw2igh_opacity", &G::Opacity, 0.15f, 1.f, "%.2f"))
			Settings::SetDirty();
		/* Draft font scale while dragging — applying live SetWindowFontScale moves the
		   grabber under the cursor and makes the slider (and pad) thrash. */
		static float sFontDraft = -1.f;
		float fontEdit = (sFontDraft > 0.f) ? sFontDraft : G::FontScale;
		if (ImGui::SliderFloat("Font scale###gw2igh_font", &fontEdit, 0.75f, 2.f, "%.2f"))
		{
			sFontDraft = fontEdit;
			G::FontScaleAuto = false;
			Settings::SetDirty();
		}
		if (ImGui::IsItemActive())
		{
			sFontDraft = fontEdit;
		}
		else if (sFontDraft > 0.f)
		{
			G::FontScale = sFontDraft;
			sFontDraft = -1.f;
			Settings::SetDirty();
		}
		PadNav::PopLabeledItemWidth();
		MutedWrap("Default 1.25x — applies the same to every panel (commits when you release).");
		if (ImGui::Checkbox("Auto font scale###gw2igh_font_auto", &G::FontScaleAuto))
		{
			sFontDraft = -1.f;
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

		ImGui::Separator();
		PadNav::Meta("GW2 API key (Live panels)");
		MutedWrap(
			"Read-only key from account.arena.net (account, progression, wallet, "
			"inventories, unlocks, tradingpost as needed). Stored only in this addon's settings.ini.");
		ImGui::SetNextItemWidth(-1.f);
		auto refreshLiveAfterKeyChange = []() {
			LivePanels::InvalidateCaches(AddonPaths::DataDir());
			const char* cur = WikiBrowser::CurrentUrlCStr();
			if (cur && (LivePanels::IsLiveUrl(cur) || LivePanels::IsLiveAbout(cur)))
				BrowserTabs::Reload();
		};
		if (ImGui::InputTextWithHint("###gw2igh_apikey", "Paste API key here...", G::Gw2ApiKey, sizeof(G::Gw2ApiKey),
				ImGuiInputTextFlags_Password | ImGuiInputTextFlags_AutoSelectAll))
		{
			Settings::SetDirty();
			refreshLiveAfterKeyChange();
		}
		if (G::Gw2ApiKey[0])
			ImGui::TextColored(HelperTheme::Ok, "Key saved - Live tabs refresh when this page is open.");
		else
			MutedWrap("No key - Vault/Progress show public data until you add one.");
		if (ImGui::Button("Clear API key###gw2igh_apikey_clear"))
		{
			G::Gw2ApiKey[0] = 0;
			Settings::SetDirty();
			refreshLiveAfterKeyChange();
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
