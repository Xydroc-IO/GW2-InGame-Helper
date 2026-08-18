#include "CraftingPad.h"

#include "AspectLayout.h"
#include "CraftingData.h"
#include "Globals.h"
#include "Gw2Catalog.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "Settings.h"
#include "WinePadOpen.h"

#include "imgui/imgui.h"

namespace
{
	bool gFocus = false;
	bool gPlaceOnce = false;
}

void CraftingPad::OpenAndRefresh()
{
	G::ShowCrafting = true;
	gFocus = true;
	gPlaceOnce = true;
	Gw2Catalog::Tick();
	(void)Gw2Catalog::RecipesReady();
	CraftingData::SetKnownDetailsActive(true);
	CraftingData::RefreshDailiesIfNeeded(false);
	Settings::SetDirty();
}

bool CraftingPad::Render()
{
	if (CraftingData::ConsumeFocusTab())
		OpenAndRefresh();
	static bool sWasOn = false;
	if (!G::ShowCrafting)
	{
		if (sWasOn)
		{
			CraftingData::SetKnownDetailsActive(false);
			sWasOn = false;
		}
		return false;
	}
	sWasOn = true;

	constexpr float kPadW = PadDock::kWorkbenchW;
	constexpr float kPadH = PadDock::kWorkbenchH;
	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(360.f);
	PadDock::SetSizeConstraints("Crafting##GW2InGameHelperCrafting", 440.f, 360.f,
		PadDock::MaxW(720.f), maxH);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.50f) : 180.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.16f) : 100.f;
		PadDock::Place(G::PadCrafting, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
	}
	if (!gPlaceOnce && G::PadCrafting.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);
	WinePadOpen::ApplyFocus(gFocus);

	bool open = G::ShowCrafting;
	HelperTheme::ScopedWindow theme(G::Opacity);
	const bool padBody = ImGui::Begin("Crafting##GW2InGameHelperCrafting", &open,
		HelperTheme::PadFlags());
	if (!theme.AfterBegin("Crafting", &open) || !padBody)
	{
		if (PadDock::Capture(G::PadCrafting))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		HelperTheme::EndPad();
		if (!open)
		{
			G::ShowCrafting = false;
			CraftingData::SetKnownDetailsActive(false);
			Settings::SetDirty();
		}
		return hovered;
	}
	if (!open)
	{
		G::ShowCrafting = false;
		CraftingData::SetKnownDetailsActive(false);
		Settings::SetDirty();
	}
	if (PadDock::Capture(G::PadCrafting))
		Settings::SetDirty();
	HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);
	CraftingData::RenderContents();
	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	HelperTheme::EndPad();
	return hovered;
}
