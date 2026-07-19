#include "UI.h"

#include "Globals.h"
#include "ItemLookup.h"
#include "Settings.h"
#include "Sites.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

namespace
{
	bool gBrowserFocused = false;
	bool gBlockGameKeyboard = false;
	bool gBlockGameMouse = false;
	bool gWikiRectValid = false;
	ImVec2 gWikiMin{};
	ImVec2 gWikiMax{};
	bool gPendingDefocus = false;

	const ImVec4 kGold(0.92f, 0.72f, 0.28f, 1.f);
	const ImVec4 kMuted(0.66f, 0.69f, 0.73f, 1.f);

	void PushWikiTheme()
	{
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.055f, 0.065f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.24f, 0.14f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.11f, 0.13f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.15f, 0.18f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.20f, 0.10f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.26f, 0.12f, 1.f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.f, 10.f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f, 5.f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 6.f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.f);
	}

	void PopWikiTheme()
	{
		ImGui::PopStyleVar(4);
		ImGui::PopStyleColor(7);
	}

	bool SoftButton(const char* label, bool enabled)
	{
		if (!enabled)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.4f);
			ImGui::Button(label);
			ImGui::PopStyleVar();
			return false;
		}
		return ImGui::Button(label);
	}

	void BlurBrowser()
	{
		if (!gBrowserFocused)
			return;
		gBrowserFocused = false;
		gBlockGameKeyboard = false;
		WikiBrowser::FeedFocus(false);
	}

	void FocusBrowser()
	{
		if (gBrowserFocused)
			return;
		gBrowserFocused = true;
		WikiBrowser::FeedFocus(true);
	}

	unsigned CefModsFromImGui(const ImGuiIO& io)
	{
		unsigned m = 0;
		if (io.KeyShift) m |= 1u << 1;
		if (io.KeyCtrl)  m |= 1u << 2;
		if (io.KeyAlt)   m |= 1u << 3;
		if (io.MouseDown[0]) m |= 1u << 4;
		if (io.MouseDown[2]) m |= 1u << 6;
		if (io.MouseDown[1]) m |= 1u << 5;
		return m;
	}

	void MapToCef(float localX, float localY, float drawW, float drawH, int* outX, int* outY)
	{
		const int fw = WikiBrowser::FrameWidth();
		const int fh = WikiBrowser::FrameHeight();
		if (fw <= 0 || fh <= 0 || drawW < 1.f || drawH < 1.f)
		{
			*outX = static_cast<int>(localX);
			*outY = static_cast<int>(localY);
			return;
		}
		int x = static_cast<int>(localX * (static_cast<float>(fw) / drawW));
		int y = static_cast<int>(localY * (static_cast<float>(fh) / drawH));
		if (x < 0) x = 0;
		if (y < 0) y = 0;
		if (x >= fw) x = fw - 1;
		if (y >= fh) y = fh - 1;
		*outX = x;
		*outY = y;
	}

	void DrawCategorizedSiteCombo(const char* comboId, float width, bool navigateOnChange)
	{
		size_t count = 0;
		const SiteDef* sites = Sites::All(&count);
		if (!sites || count == 0)
			return;

		const int current = Sites::ActiveIndex();
		const SiteDef& active = sites[current];
		char preview[96];
		std::snprintf(preview, sizeof(preview), "%s · %s",
			active.category ? active.category : "",
			active.label ? active.label : "");

		ImGui::SetNextItemWidth(width);
		if (!ImGui::BeginCombo(comboId, preview))
			return;

		const char* lastCategory = nullptr;
		for (int i = 0; i < static_cast<int>(count); ++i)
		{
			const char* cat = sites[i].category ? sites[i].category : "";
			if (!lastCategory || std::strcmp(lastCategory, cat) != 0)
			{
				if (lastCategory)
					ImGui::Spacing();
				ImGui::PushStyleColor(ImGuiCol_Text, kGold);
				ImGui::TextUnformatted(cat);
				ImGui::PopStyleColor();
				ImGui::Separator();
				lastCategory = cat;
			}

			ImGui::PushID(i);
			const bool selected = (i == current);
			if (ImGui::Selectable(sites[i].label, selected))
			{
				if (!selected && Sites::SetActiveIndex(i))
				{
					std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "%s", Sites::ActiveId());
					Settings::SetDirty();
					if (navigateOnChange)
						WikiBrowser::NavigateActiveSite();
				}
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}

	void DrawSitePicker()
	{
		DrawCategorizedSiteCombo("##site_picker", 200.f, true);
	}
}

bool UI_BlocksGameKeyboard()
{
	return gBlockGameKeyboard;
}

bool UI_BlocksGameMouse()
{
	return gBlockGameMouse;
}

bool UI_IsPointerOverWiki(int clientX, int clientY)
{
	if (!G::ShowWiki || !gWikiRectValid)
		return false;
	const float x = static_cast<float>(clientX);
	const float y = static_cast<float>(clientY);
	return x >= gWikiMin.x && y >= gWikiMin.y && x < gWikiMax.x && y < gWikiMax.y;
}

void UI_ReleaseGameInput()
{
	BlurBrowser();
	gBlockGameMouse = false;
	gPendingDefocus = true;
}

void UI_Render()
{
	/* Always poll first — must run while the helper is closed too. */
	HelperHotkeys_Poll();

	gBlockGameKeyboard = false;
	gBlockGameMouse = false;
	gWikiRectValid = false;

	if (gPendingDefocus)
	{
		gPendingDefocus = false;
		ImGui::SetWindowFocus(nullptr);
	}

	if (!G::ShowWiki)
	{
		BlurBrowser();
		WikiBrowser::SetVisible(false);
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(G::WindowWidth, G::WindowHeight), ImGuiCond_FirstUseEver);
	if (G::HasSavedPos)
		ImGui::SetNextWindowPos(ImVec2(G::WindowPosX, G::WindowPosY), ImGuiCond_FirstUseEver);

	PushWikiTheme();
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, G::Opacity);
	ImGui::SetNextWindowBgAlpha(G::Opacity);

	bool open = G::ShowWiki;
	if (!ImGui::Begin("In-Game Helper##GW2InGameHelper", &open))
	{
		WikiBrowser::SetVisible(false);
		ImGui::End();
		ImGui::PopStyleVar();
		PopWikiTheme();
		return;
	}
	if (!open)
	{
		G::ShowWiki = false;
		Settings::SetDirty();
		WikiBrowser::SetVisible(false);
		BlurBrowser();
	}

	ImGui::SetWindowFontScale(G::FontScale);

	ImGui::TextColored(kGold, "IN-GAME HELPER");
	ImGui::SameLine();
	DrawSitePicker();
	ImGui::SameLine();
	if (SoftButton("<", WikiBrowser::CanGoBack()))
		WikiBrowser::GoBack();
	ImGui::SameLine();
	if (SoftButton(">", WikiBrowser::CanGoForward()))
		WikiBrowser::GoForward();
	ImGui::SameLine();
	if (ImGui::Button("Home"))
		WikiBrowser::NavigateHome();
	ImGui::SameLine();
	if (ImGui::Button("Reload"))
		WikiBrowser::Reload();
	ImGui::SameLine();
	ImGui::TextColored(kMuted, "%s", WikiBrowser::Status().c_str());

	if (Sites::Active().itemLookup)
	{
		const std::string itemStatus = ItemLookup::Status();
		if (!itemStatus.empty())
		{
			ImGui::SameLine();
			ImGui::TextColored(kGold, "· %s", itemStatus.c_str());
		}
	}

	const std::string url = WikiBrowser::CurrentUrl();
	if (!url.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, kMuted);
		ImGui::TextUnformatted(url.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::Separator();

	const ImVec2 avail = ImGui::GetContentRegionAvail();
	ImGui::BeginChild("##wiki_osr_slot", avail, false,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoNavInputs);

	const ImVec2 imageSize = ImGui::GetContentRegionAvail();
	if (G::ShowWiki && imageSize.x > 40.f && imageSize.y > 40.f)
	{
		WikiBrowser::SetVisible(true);
		WikiBrowser::SetBounds(0.f, 0.f, imageSize.x, imageSize.y);
		WikiBrowser::PresentFrame();
	}
	else
		WikiBrowser::SetVisible(false);

	ImGuiIO& io = ImGui::GetIO();
	bool overPage = false;

	if (WikiBrowser::HasFrame())
	{
		const ImVec2 cursor = ImGui::GetCursorScreenPos();
		ImGui::Image(reinterpret_cast<ImTextureID>(WikiBrowser::FrameSrv()), imageSize);

		ImGui::SetCursorScreenPos(cursor);
		ImGui::InvisibleButton("##wiki_hit", imageSize,
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
			ImGuiButtonFlags_MouseButtonMiddle);

		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();
		overPage = hovered || active;
		const unsigned mods = CefModsFromImGui(io);

		if (overPage)
		{
			const ImVec2 mouse = io.MousePos;
			const float localX = mouse.x - cursor.x;
			const float localY = mouse.y - cursor.y;
			int cx = 0, cy = 0;
			MapToCef(localX, localY, imageSize.x, imageSize.y, &cx, &cy);

			WikiBrowser::FeedMouseMove(cx, cy, false, mods);

			auto click = [&](ImGuiMouseButton btn, int cefBtn) {
				if (ImGui::IsMouseClicked(btn))
				{
					FocusBrowser();
					WikiBrowser::FeedMouseClick(cx, cy, cefBtn, false,
						ImGui::IsMouseDoubleClicked(btn) ? 2 : 1, mods);
				}
				if (ImGui::IsMouseReleased(btn))
					WikiBrowser::FeedMouseClick(cx, cy, cefBtn, true, 1, mods);
			};
			click(ImGuiMouseButton_Left, 0);
			click(ImGuiMouseButton_Right, 2);
			click(ImGuiMouseButton_Middle, 1);

			if (io.MouseWheel != 0.f || io.MouseWheelH != 0.f)
			{
				WikiBrowser::FeedMouseWheel(cx, cy,
					static_cast<int>(io.MouseWheelH * 120.f),
					static_cast<int>(io.MouseWheel * 120.f),
					mods);
			}
		}
	}
	else
	{
		ImGui::Spacing();
		ImGui::TextColored(kGold, WikiBrowser::IsReady()
			? "Waiting for first paint…"
			: "Loading browser…");
		ImGui::TextWrapped("%s", WikiBrowser::Status().c_str());
	}

	ImGui::EndChild();

	const ImVec2 pos = ImGui::GetWindowPos();
	const ImVec2 winSize = ImGui::GetWindowSize();
	gWikiMin = pos;
	gWikiMax = ImVec2(pos.x + winSize.x, pos.y + winSize.y);
	gWikiRectValid = true;

	const bool mouseOverWiki = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

	gBlockGameMouse = G::ShowWiki && (overPage || mouseOverWiki);
	gBlockGameKeyboard = G::ShowWiki && gBrowserFocused;

	if (gBlockGameMouse)
		io.WantCaptureMouse = true;
	if (gBlockGameKeyboard)
		io.WantCaptureKeyboard = true;

	if (pos.x != G::WindowPosX || pos.y != G::WindowPosY ||
		winSize.x != G::WindowWidth || winSize.y != G::WindowHeight)
	{
		G::WindowPosX = pos.x;
		G::WindowPosY = pos.y;
		G::WindowWidth = winSize.x;
		G::WindowHeight = winSize.y;
		G::HasSavedPos = true;
		Settings::SetDirty();
	}

	ImGui::SetWindowFontScale(1.f);
	ImGui::End();
	ImGui::PopStyleVar();
	PopWikiTheme();
	Settings::Save();
}

void UI_Options()
{
	ImGui::TextUnformatted("GW2 In-Game Helper");
	ImGui::Separator();
	if (ImGui::Checkbox("Show helper window", &G::ShowWiki))
		Settings::SetDirty();

	size_t count = 0;
	Sites::All(&count);
	if (count > 0)
		DrawCategorizedSiteCombo("Default site", 260.f, G::ShowWiki);

	if (ImGui::SliderFloat("Opacity", &G::Opacity, 0.15f, 1.f, "%.2f"))
		Settings::SetDirty();
	if (ImGui::SliderFloat("Font scale", &G::FontScale, 0.75f, 2.f, "%.2f"))
		Settings::SetDirty();

	ImGui::Spacing();
	ImGui::TextWrapped(
		"Pick a site from the toolbar dropdown, then browse in-game. Click outside "
		"the window to move and use skills again — you do not need to close the addon.");
	ImGui::TextWrapped("Hotkeys: Ctrl+Shift+H (or K) toggle · Ctrl+Shift+U wiki item from clipboard [&…]");
	ImGui::TextColored(kMuted, "%s", WikiBrowser::Status().c_str());
	if (Sites::Active().itemLookup)
		ImGui::TextColored(kMuted, "Item: %s", ItemLookup::Status().c_str());
}
