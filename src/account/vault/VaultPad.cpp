#include "VaultPad.h"

#include "VaultPadInternal.h"

#include "AspectLayout.h"
#include "BgFetch.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadLayout.h"
#include "PadNav.h"
#include "PadDock.h"
#include "Settings.h"
#include "WinePadOpen.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

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
	std::atomic<bool> gDeferredFetch{false};
	std::atomic<bool> gDeferredForce{false};
	HANDLE gThread = nullptr;
	bool gFocus = false;
	bool gPlaceOnce = false;
	int  gDeferRefresh = 0;
	int  gBoard = 0; /* 0 all, 1 daily, 2 weekly, 3 special */
	bool gHideDone = false;

	int CountDone(const std::vector<Obj>& list)
	{
		int n = 0;
		for (const Obj& o : list)
		{
			if (o.done)
				++n;
		}
		return n;
	}

	int CountLeft(const std::vector<Obj>& list)
	{
		return static_cast<int>(list.size()) - CountDone(list);
	}

	void DrawResetCountdowns()
	{
		const std::string daily = FormatCountdown(SecUntilDailyResetUtc());
		const std::string weekly = FormatCountdown(SecUntilWeeklyResetUtc());
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::GoldMuted, "Daily %s  ·  Weekly %s",
			daily.c_str(), weekly.c_str());
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

	void DrawObjRow(const Obj& o, int id)
	{
		ImGui::PushID(id);
		char acclaim[32];
		acclaim[0] = 0;
		ImVec4 valCol = HelperTheme::GoldMuted;
		if (o.done)
		{
			std::snprintf(acclaim, sizeof(acclaim), "Done");
			valCol = HelperTheme::Ok;
		}
		else if (o.acclaim > 0)
			std::snprintf(acclaim, sizeof(acclaim), "%d acclaim", o.acclaim);

		const char* chip = o.track.empty() ? nullptr : o.track.c_str();
		const ImVec4 chipFill = o.done
			? ImVec4(0.16f, 0.28f, 0.14f, 1.f)
			: HelperTheme::Header;
		const ImVec4 chipText = o.done ? HelperTheme::Ok : HelperTheme::GoldBright;
		PadLayout::TitleRow(chip, chipFill, chipText, o.title.c_str(), acclaim, valCol);

		if (o.need > 0)
		{
			float frac = static_cast<float>(o.cur) / static_cast<float>(o.need);
			if (frac < 0.f) frac = 0.f;
			if (frac > 1.f) frac = 1.f;
			char prog[24];
			std::snprintf(prog, sizeof(prog), "%d / %d", o.cur, o.need);
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
				o.done ? HelperTheme::Ok : HelperTheme::GoldDim);
			ImGui::ProgressBar(frac, ImVec2(-FLT_MIN, 10.f), prog);
			ImGui::PopStyleColor();
		}
		ImGui::Spacing();
		ImGui::PopID();
	}

	void DrawObjList(const char* label, const char* id, const std::vector<Obj>& list)
	{
		const int done = CountDone(list);
		const int total = static_cast<int>(list.size());
		const int left = total - done;
		char hdr[96];
		if (total <= 0)
			std::snprintf(hdr, sizeof(hdr), "%s###%s", label, id);
		else if (left <= 0)
			std::snprintf(hdr, sizeof(hdr), "%s  all done###%s", label, id);
		else
			std::snprintf(hdr, sizeof(hdr), "%s  %d left  (%d / %d)###%s",
				label, left, done, total, id);

		if (!ImGui::TreeNodeEx(hdr, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
			return;
		if (list.empty())
		{
			ImGui::TextColored(HelperTheme::Muted, "No objectives.");
			ImGui::TreePop();
			return;
		}

		int shown = 0;
		for (size_t i = 0; i < list.size(); ++i)
		{
			if (list[i].done)
				continue;
			DrawObjRow(list[i], static_cast<int>(i));
			++shown;
		}
		if (!gHideDone)
		{
			for (size_t i = 0; i < list.size(); ++i)
			{
				if (!list[i].done)
					continue;
				DrawObjRow(list[i], static_cast<int>(i));
				++shown;
			}
		}
		if (shown == 0)
			ImGui::TextColored(HelperTheme::Ok, "Nothing left in this board.");
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
	PadDock::ClearCustomCollapsed("Dailies & Vault##GW2InGameHelperVault");
	gDeferRefresh = WinePadOpen::DeferFrames();
	if (gDeferRefresh <= 0)
		RefreshData();
}

void VaultPad::RenderContents()
{
	if (WinePadOpen::TickDefer(gDeferRefresh))
		RefreshData();
	BgFetch::SetWanted(BgFetch::Channel::Vault, true);
	TickDeferredFetch();
	SyncDraw();
	const Snapshot& snap = gDraw;

	if (PadNav::RefreshButton("###gw2igh_vault_ref"))
		StartFetch(true);
	ImGui::SameLine();
	if (gBusy)
		PadNav::StatusBusy();
	else if (snap.hasKey && !snap.scopeFail)
	{
		const int dLeft = CountLeft(snap.daily);
		const int wLeft = CountLeft(snap.weekly);
		const int sLeft = CountLeft(snap.special);
		char remain[96];
		std::snprintf(remain, sizeof(remain), "Daily %d left  ·  Weekly %d left  ·  Special %d left",
			dLeft, wLeft, sLeft);
		if (dLeft + wLeft + sLeft == 0)
			PadNav::StatusOk("All vault objectives done");
		else
			PadNav::StatusWarn(remain);
	}
	else if (!snap.status.empty())
		PadNav::StatusOk(snap.status.c_str());

	ImGui::Separator();

	PadLayout::Hero("###gw2igh_vault_hero",
		snap.seasonTitle.empty() ? "Wizard's Vault" : snap.seasonTitle.c_str(),
		snap.seasonBlurb.c_str(),
		"");
	DrawResetCountdowns();

	if (PadLayout::GoldButton("All###gw2igh_vault_all", gBoard == 0, true))
		gBoard = 0;
	if (PadLayout::GoldButton("Daily###gw2igh_vault_d", gBoard == 1, false))
		gBoard = 1;
	if (PadLayout::GoldButton("Weekly###gw2igh_vault_w", gBoard == 2, false))
		gBoard = 2;
	if (PadLayout::GoldButton("Special###gw2igh_vault_s", gBoard == 3, false))
		gBoard = 3;
	PadNav::WrapSameLine(PadNav::CheckboxWidth("Hide done"));
	ImGui::Checkbox("Hide done###gw2igh_vault_hide", &gHideDone);

	PadLayout::BeginList("###gw2igh_vault_list", 80.f);

	if (!snap.hasKey)
	{
		PadNav::Blurb("Add an API key in Settings (account + progression) for live personal Vault.");
		DrawObjList("Easy preview", "vault_easy", snap.easyPreview);
	}
	else if (snap.scopeFail)
	{
		PadNav::Blurb("API key needs account + progression scopes for live Vault progress.");
	}
	else
	{
		if (gBoard == 0 || gBoard == 1)
			DrawObjList("Daily", "vault_daily", snap.daily);
		if (gBoard == 0 || gBoard == 2)
			DrawObjList("Weekly", "vault_weekly", snap.weekly);
		if (gBoard == 0 || gBoard == 3)
			DrawObjList("Special", "vault_special", snap.special);
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
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);
	WinePadOpen::ApplyFocus(gFocus);

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
