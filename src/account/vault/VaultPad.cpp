#include "VaultPad.h"

#include "VaultPadInternal.h"

#include "AspectLayout.h"
#include "Globals.h"
#include "HelperTheme.h"
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
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(ImVec4(0.75f, 0.82f, 0.95f, 1.f),
			"Daily reset in %s", daily.c_str());
		ImGui::TextColored(ImVec4(0.75f, 0.82f, 0.95f, 1.f),
			"Weekly reset in %s", weekly.c_str());
		ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f),
			"UTC — daily 00:00 · weekly Mon 07:30");
		ImGui::PopTextWrapPos();
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
			ImGui::PushTextWrapPos(0.f);
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "No objectives.");
			ImGui::PopTextWrapPos();
			ImGui::TreePop();
			return;
		}
		for (size_t i = 0; i < list.size(); ++i)
		{
			const Obj& o = list[i];
			ImGui::PushID(static_cast<int>(i));
			ImVec4 col = o.done
				? ImVec4(0.45f, 0.75f, 0.50f, 1.f)
				: ImVec4(0.88f, 0.88f, 0.90f, 1.f);
			ImGui::PushTextWrapPos(0.f);
			ImGui::PushStyleColor(ImGuiCol_Text, col);
			ImGui::TextWrapped("%s%s", o.done ? "[Done] " : "", o.title.c_str());
			ImGui::PopStyleColor();
			if (!o.track.empty() || o.acclaim > 0 || o.need > 0)
			{
				std::string m;
				if (!o.track.empty()) m += o.track;
				if (o.acclaim > 0)
				{
					if (!m.empty()) m += " · ";
					m += std::to_string(o.acclaim);
					m += " acclaim";
					if (o.acclaim <= 10) m += " (easy)";
				}
				if (o.need > 0)
				{
					if (!m.empty()) m += " · ";
					m += std::to_string(o.cur);
					m += " / ";
					m += std::to_string(o.need);
				}
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.62f, 1.f));
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
			ImGui::PopTextWrapPos();
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

	ImGui::TextUnformatted("Dailies & Wizard’s Vault");
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
		"Official API — account + progression scopes. (Account → Vault tab.)");
	ImGui::PopTextWrapPos();

	if (ImGui::Button("Refresh###gw2igh_vault_ref"))
		StartFetch(true);
	if (gBusy)
	{
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "Updating…");
		ImGui::PopTextWrapPos();
	}
	else if (!snap.status.empty())
	{
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", snap.status.c_str());
		ImGui::PopTextWrapPos();
	}

	ImGui::Separator();

	const float listH = ImGui::GetContentRegionAvail().y;
	ImGui::BeginChild("###gw2igh_vault_list", ImVec2(0.f, listH > 80.f ? listH : 80.f), true);

	ImGui::PushTextWrapPos(0.f);
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.78f, 0.35f, 1.f));
	ImGui::TextWrapped("%s", snap.seasonTitle.c_str());
	ImGui::PopStyleColor();
	ImGui::TextWrapped("%s", snap.seasonBlurb.c_str());
	ImGui::PopTextWrapPos();
	ImGui::Spacing();
	DrawResetCountdowns();
	ImGui::Spacing();

	if (!snap.hasKey)
	{
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextWrapped("Add an API key in Settings (helper side rail; account + progression) for live personal Vault.");
		ImGui::PopTextWrapPos();
		DrawObjList("Easy Vault preview", snap.easyPreview);
	}
	else if (snap.scopeFail)
	{
		ImGui::PushTextWrapPos(0.f);
		ImGui::TextWrapped("API key needs account + progression scopes for live Vault progress.");
		ImGui::PopTextWrapPos();
	}
	else
	{
		DrawObjList("Daily Vault", snap.daily);
		DrawObjList("Weekly Vault", snap.weekly);
		DrawObjList("Special Vault", snap.special);
	}

	ImGui::EndChild();
}

bool VaultPad::Render()
{
	if (!G::ShowVault)
		return false;

	const ImGuiIO& io = ImGui::GetIO();
	const float maxH = PadDock::MaxH(280.f);
	ImGui::SetNextWindowSizeConstraints(ImVec2(360.f, 280.f), ImVec2(PadDock::MaxW(560.f), maxH));
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
	if (!ImGui::Begin("Dailies & Vault##GW2InGameHelperVault", &open))
	{
		if (PadDock::Capture(G::PadVault))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		ImGui::End();
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

	HelperTheme::ScopedFontScale fontScale;
	RenderContents();

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	ImGui::End();
	return hovered;
}
