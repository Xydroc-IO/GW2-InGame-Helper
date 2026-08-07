#include "VaultPad.h"

#include "VaultPadInternal.h"

#include "AspectLayout.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadLayout.h"
#include "PadNav.h"
#include "PadDock.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <atomic>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

namespace VaultDetail
{
	std::mutex gMu;
	Snapshot gSnap;
	Snapshot gDraw;
	std::atomic<unsigned> gGen{0};
	unsigned gDrawnGen = 0;
	std::atomic<bool> gBusy{false};
	HANDLE gThread = nullptr;
	bool gFocus = false;
	bool gPlaceOnce = false;

	void DrawResetCountdowns()
	{
		const std::string daily = FormatCountdown(SecUntilDailyResetUtc());
		const std::string weekly = FormatCountdown(SecUntilWeeklyResetUtc());
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::GoldMuted,
			"Daily reset in %s", daily.c_str());
		ImGui::TextColored(HelperTheme::GoldMuted,
			"Weekly reset in %s", weekly.c_str());
		ImGui::TextColored(HelperTheme::Muted,
			"UTC - daily 00:00 | weekly Mon 07:30");
		PadNav::PopWrap();
	}

	void SyncDraw()
	{
		const unsigned gen = gGen.load();
		if (gen == gDrawnGen) return;
		std::lock_guard<std::mutex> lock(gMu);
		gDraw = gSnap;
		gDrawnGen = gGen.load();
	}

	void DrawObjList(const char* label, const std::vector<Obj>& list)
	{
		if (!ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
			return;
		if (list.empty())
		{
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted, "No objectives.");
			PadNav::PopWrap();
			ImGui::TreePop();
			return;
		}
		for (size_t i = 0; i < list.size(); ++i)
		{
			const Obj& o = list[i];
			ImGui::PushID(static_cast<int>(i));
			const ImVec4 col = o.done ? HelperTheme::Ok : HelperTheme::Ink;
			PadNav::PushWrap();
			ImGui::PushStyleColor(ImGuiCol_Text, col);
			ImGui::TextWrapped("%s%s", o.done ? "[Done] " : "", o.title.c_str());
			ImGui::PopStyleColor();
			if (!o.track.empty() || o.acclaim > 0 || o.need > 0)
			{
				std::string m;
				if (!o.track.empty()) m += o.track;
				if (o.acclaim > 0)
				{
					if (!m.empty()) m += " | ";
					m += std::to_string(o.acclaim);
					m += " acclaim";
					if (o.acclaim <= 10) m += " (easy)";
				}
				if (o.need > 0)
				{
					if (!m.empty()) m += " | ";
					m += std::to_string(o.cur);
					m += " / ";
					m += std::to_string(o.need);
				}
				ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::Muted);
				ImGui::TextWrapped("%s", m.c_str());
				ImGui::PopStyleColor();
				if (o.need > 0)
				{
					float frac = static_cast<float>(o.cur) / static_cast<float>(o.need);
					if (frac < 0.f) frac = 0.f;
					if (frac > 1.f) frac = 1.f;
					ImGui::ProgressBar(frac, ImVec2(-FLT_MIN, 6.f), "");
				}
			}
			PadNav::PopWrap();
			ImGui::Spacing();
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
} // namespace VaultDetail

using namespace VaultDetail;

void VaultPad::RefreshData()
{
	bool need = true;
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (gSnap.ok && gSnap.fetchedAt != 0 &&
			(GetTickCount() - gSnap.fetchedAt) < kCacheTtlMs)
			need = false;
	}
	StartFetch(need);
}

void VaultPad::OpenAndRefresh()
{
	G::ShowVault = true;
	gFocus = true;
	gPlaceOnce = true;
	Settings::SetDirty();
	RefreshData();
}

void VaultPad::RenderContents()
{
	SyncDraw();
	const Snapshot& snap = gDraw;

	PadNav::Blurb("Wizard's Vault from the official API. Today board: Browse live-dailies. Offline sheet is reference only.");

	if (PadNav::RefreshButton("###gw2igh_vault_ref"))
		StartFetch(true);
	ImGui::SameLine();
	if (gBusy)
		PadNav::StatusBusy();
	else if (!snap.status.empty())
		PadNav::StatusOk(snap.status.c_str());

	ImGui::Separator();

	PadLayout::BeginList("###gw2igh_vault_list", 80.f);

	PadNav::PushWrap();
	ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::Gold);
	ImGui::TextWrapped("%s", snap.seasonTitle.c_str());
	ImGui::PopStyleColor();
	ImGui::TextColored(HelperTheme::Muted, "%s", snap.seasonBlurb.c_str());
	PadNav::PopWrap();
	ImGui::Spacing();
	DrawResetCountdowns();
	ImGui::Spacing();

	if (!snap.hasKey)
	{
		PadNav::Blurb("Add an API key in Settings (helper side rail; account + progression) for live personal Vault.");
		DrawObjList("Easy Vault preview", snap.easyPreview);
	}
	else if (snap.scopeFail)
	{
		PadNav::Blurb("API key needs account + progression scopes for live Vault progress.");
	}
	else
	{
		DrawObjList("Daily Vault", snap.daily);
		DrawObjList("Weekly Vault", snap.weekly);
		DrawObjList("Special Vault", snap.special);
	}

	PadLayout::EndList();
}

bool VaultPad::Render()
{
	if (!G::ShowVault)
		return false;

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(280.f);
	PadDock::SetSizeConstraints("Dailies & Vault##GW2InGameHelperVault", 360.f, 280.f, PadDock::MaxW(560.f), maxH);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	{
		const float fx = (io.DisplaySize.x > 100.f)
			? AspectLayout::PadFallbackX(io.DisplaySize.x, io.DisplaySize.y, 0.38f) : 100.f;
		const float fy = (io.DisplaySize.y > 100.f)
			? AspectLayout::PadFallbackY(io.DisplaySize.y, 0.12f) : 80.f;
		PadDock::Place(G::PadVault, gPlaceOnce, kPadW, kPadH, ImVec2(fx, fy));
	}
	if (!gPlaceOnce && G::PadVault.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);
	if (gFocus)
	{
		ImGui::SetNextWindowFocus();
		gFocus = false;
	}

	bool open = G::ShowVault;
	HelperTheme::ScopedWindow theme(G::Opacity);
	const bool padBody = ImGui::Begin("Dailies & Vault##GW2InGameHelperVault", &open, HelperTheme::PadFlags());
	if (!theme.AfterBegin("Dailies & Vault", &open) || !padBody)
	{
		if (PadDock::Capture(G::PadVault))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		HelperTheme::EndPad();
		if (!open)
		{
			G::ShowVault = false;
			Settings::SetDirty();
		}
		return hovered;
	}

	if (!open)
	{
		G::ShowVault = false;
		Settings::SetDirty();
	}
	if (PadDock::Capture(G::PadVault))
		Settings::SetDirty();

	HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);
	RenderContents();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	HelperTheme::EndPad();
	return hovered;
}
