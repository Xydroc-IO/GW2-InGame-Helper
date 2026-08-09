#include "WatchPad.h"
#include "WatchPadInternal.h"

#include "EiRuntime.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "PadNav.h"
#include "Settings.h"
#include "WatchCapture.h"
#include "WatchLinux.h"
#include "WinePadOpen.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <d3d11.h>

namespace
{
	bool gRequestDock = false;
	int  gControlTab = 0;

	ImU32 ColU32(const ImVec4& c, float aMul = 1.f)
	{
		ImVec4 t = c;
		t.w *= aMul;
		return ImGui::ColorConvertFloat4ToU32(t);
	}

	void PaintStageFrame(ImDrawList* dl, ImVec2 p0, ImVec2 p1, bool live)
	{
		if (!dl)
			return;
		const float a = G::Opacity > 0.15f ? G::Opacity : 0.97f;
		dl->AddRectFilled(p0, p1, IM_COL32(4, 3, 2, static_cast<int>(235 * a + 0.5f)));
		dl->AddRect(p0, p1, ColU32(HelperTheme::GoldDim, 0.85f * a), 0.f, 0, 1.6f);
		const ImVec2 i0(p0.x + 3.f, p0.y + 3.f);
		const ImVec2 i1(p1.x - 3.f, p1.y - 3.f);
		if (i1.x > i0.x && i1.y > i0.y)
			dl->AddRect(i0, i1, ColU32(HelperTheme::GoldBright, 0.55f * a), 0.f, 0, 1.f);

		const float tick = 10.f;
		const ImU32 tickCol = ColU32(HelperTheme::Gold, 0.9f * a);
		auto corner = [&](float x, float y, float dx, float dy) {
			dl->AddLine(ImVec2(x, y), ImVec2(x + dx * tick, y), tickCol, 1.8f);
			dl->AddLine(ImVec2(x, y), ImVec2(x, y + dy * tick), tickCol, 1.8f);
		};
		corner(p0.x + 1.f, p0.y + 1.f, 1.f, 1.f);
		corner(p1.x - 1.f, p0.y + 1.f, -1.f, 1.f);
		corner(p0.x + 1.f, p1.y - 1.f, 1.f, -1.f);
		corner(p1.x - 1.f, p1.y - 1.f, -1.f, -1.f);

		if (live)
		{
			const float r = 4.f;
			const ImVec2 c(p0.x + 14.f, p0.y + 12.f);
			dl->AddCircleFilled(c, r, ColU32(HelperTheme::Ok, a));
			dl->AddCircle(c, r + 1.5f, ColU32(HelperTheme::GoldBright, 0.7f * a), 0, 1.2f);
		}
	}

	bool RenderMirror()
	{
		if (!G::ShowWatchMirror)
			return false;

		constexpr float kPadW = 960.f;
		constexpr float kPadH = 540.f;
		PadDock::SetSizeConstraints("Watch Mirror###GW2InGameHelperWatchMirror", 480.f, 300.f,
			PadDock::MaxW(1600.f), PadDock::MaxH(1000.f));
		ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
		PadDock::Place(G::PadWatchMirror, WatchPadDetail::gRequestMirrorDock, kPadW, kPadH,
			PadDock::BesideHelper(kPadW));
		if (!WatchPadDetail::gRequestMirrorDock && (G::PadWatchMirror.w < 80.f || G::PadWatchMirror.h < 80.f))
			ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);

		bool open = G::ShowWatchMirror;
		HelperTheme::ScopedWindow theme(G::Opacity);
		const bool padBody = ImGui::Begin("Watch Mirror###GW2InGameHelperWatchMirror", &open,
			HelperTheme::PadFlags());
		if (!theme.AfterBegin("Watch Mirror", &open) || !padBody)
		{
			if (PadDock::Capture(G::PadWatchMirror))
				Settings::SetDirty();
			const bool hovered = ImGui::IsWindowHovered(
				ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
				ImGuiHoveredFlags_ChildWindows);
			HelperTheme::EndPad();
			if (!open)
			{
				G::ShowWatchMirror = false;
				WatchCapture::Stop();
				Settings::SetDirty();
			}
			return hovered;
		}

		HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);
		ImGui::PushID("gw2igh_watch_mirror");

		const bool capturing = WatchCapture::IsCapturing();
		constexpr float kPad = 6.f;
		constexpr float kInset = 8.f;
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		const float stageW = (std::max)(40.f, avail.x);
		const float stageH = (std::max)(96.f, avail.y - 2.f);
		const ImVec2 stageCursor = ImGui::GetCursorScreenPos();
		ImGui::Dummy(ImVec2(stageW, stageH));
		const ImVec2 stage0 = stageCursor;
		const ImVec2 stage1(stageCursor.x + stageW, stageCursor.y + stageH);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		PaintStageFrame(dl, stage0, stage1, capturing);

		const uint32_t cw = WatchCapture::ContentW();
		const uint32_t ch = WatchCapture::ContentH();
		ID3D11ShaderResourceView* srv = WatchCapture::Srv();
		const ImVec2 inner0(stage0.x + kPad, stage0.y + kPad);
		const ImVec2 inner1(stage1.x - kPad, stage1.y - kPad);
		const float innerW = inner1.x - inner0.x;
		const float innerH = inner1.y - inner0.y;
		if (srv && cw > 0 && ch > 0 && innerW > 8.f && innerH > 8.f)
		{
			const float aspect = static_cast<float>(cw) / static_cast<float>(ch);
			float drawW = innerW;
			float drawH = drawW / aspect;
			if (drawH > innerH)
			{
				drawH = innerH;
				drawW = drawH * aspect;
			}
			const float ox = (innerW - drawW) * 0.5f;
			const float oy = (innerH - drawH) * 0.5f;
			const ImVec2 img0(inner0.x + ox, inner0.y + oy);
			const ImVec2 img1(img0.x + drawW, img0.y + drawH);
			if (drawW > 1.f && drawH > 1.f && dl)
			{
				dl->AddImage(reinterpret_cast<ImTextureID>(srv), img0, img1);
				dl->AddRect(img0, img1, ColU32(HelperTheme::GoldDim, 0.35f * G::Opacity), 0.f, 0, 1.f);
			}
		}
		else
		{
			ImGui::SetCursorScreenPos(ImVec2(inner0.x + kInset, inner0.y + kInset));
			ImGui::PushTextWrapPos(inner1.x - kInset);
			ImGui::TextColored(HelperTheme::Muted, "%s",
				capturing
					? (EiRuntime::IsWine()
						? "Waiting for portal / first frame…"
						: "Waiting for first frame…")
					: "Stopped — use Start on the Watch pad.");
			ImGui::PopTextWrapPos();
		}

		ImGui::PopID();
		if (PadDock::Capture(G::PadWatchMirror))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
			ImGuiHoveredFlags_ChildWindows);
		HelperTheme::EndPad();
		WatchPadDetail::gRequestMirrorDock = false;
		if (!open)
		{
			G::ShowWatchMirror = false;
			WatchCapture::Stop();
			Settings::SetDirty();
		}
		return hovered;
	}
}

namespace WatchPadDetail
{
	bool gRequestMirrorDock = false;
	int  gDeferMirrorOpenFrames = 0;
}

void WatchPad::Open()
{
	G::ShowWatch = true;
	gRequestDock = true;
	gControlTab = 0;
	/* Do not EnumWindows/GetWindowText on the ImGui click frame — that has
	   asserted Size > 0 on native Windows. List refreshes via EnsureList / Refresh. */
	if (!G::ShowWatchMirror && WatchCapture::IsCapturing())
		WatchCapture::Stop(); /* drop orphan picker/session left after other pads */
	if (!WatchCapture::IsStreaming())
		WatchPadDetail::gSelected = -1;
	if (G::PadWatch.w < 80.f || G::PadWatch.h < 80.f)
	{
		G::PadWatch.w = 0.f;
		G::PadWatch.h = 0.f;
	}
	/* Clear sticky minimize so reopen after other pads isn't a dead title strip. */
	if (ImGuiWindow* w = ImGui::FindWindowByName("Watch###GW2InGameHelperWatch"))
		w->StateStorage.SetBool(w->GetID("##gw2igh_pad_collapsed"), false);
	/* Do NOT WarmAsync/CreateThread on the side-rail click frame — that has
	   taken down Wine under load (compass + trails). watchd warms on Start. */
	Settings::SetDirty();
}

void WatchPad::CloseAll()
{
	WinePadOpen::CancelWatchOpen();
	G::ShowWatch = false;
	G::ShowWatchMirror = false;
	WatchPadDetail::gDeferMirrorOpenFrames = 0;
	WatchCapture::Stop();
	Settings::SetDirty();
}

void WatchPad::ToggleControl()
{
	if (G::ShowWatch)
	{
		WinePadOpen::CancelWatchOpen();
		G::ShowWatch = false;
		/* Closing the last Watch UI while idle/picker: drop sticky capture state. */
		if (!G::ShowWatchMirror && WatchPadDetail::gDeferMirrorOpenFrames <= 0)
			WatchCapture::Stop();
		Settings::SetDirty();
		return;
	}
	/* Wine under load (API pads / CEF already up): do not Begin Watch on the
	   same ImGui click frame as the side-rail toggle. */
	if (WinePadOpen::Soft())
	{
		WinePadOpen::QueueWatchOpen();
		return;
	}
	Open();
}

bool WatchPad::Render()
{
	bool hovered = false;
	try
	{
		WatchPadDetail::TickDeferredMirrorOpen();

		/* Always Tick so deferred/parked SRV free runs even after Mirror closed
		   (Stop queues Release; Wine still needs a later frame to drop it). */
		WatchCapture::Tick();

		/* Mirror opens only from Start (OpenMirror). Do NOT auto-reopen here —
		   that fought closed Mirrors / other pads whenever streaming state was sticky. */

		if (G::ShowWatch)
		{
			constexpr float kPadW = 440.f;
			constexpr float kPadH = 320.f;

			PadDock::SetSizeConstraints("Watch###GW2InGameHelperWatch", 320.f, 200.f,
				PadDock::MaxW(640.f), PadDock::MaxH(640.f));
			ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
			PadDock::Place(G::PadWatch, gRequestDock, kPadW, kPadH, PadDock::BesideHelper(kPadW));
			if (!gRequestDock && (G::PadWatch.w < 80.f || G::PadWatch.h < 80.f))
				ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);

			bool open = G::ShowWatch;
			HelperTheme::ScopedWindow theme(G::Opacity);
			const bool padBody = ImGui::Begin("Watch###GW2InGameHelperWatch", &open, HelperTheme::PadFlags());
			if (!theme.AfterBegin("Watch", &open) || !padBody)
			{
				if (PadDock::Capture(G::PadWatch))
					Settings::SetDirty();
				hovered |= ImGui::IsWindowHovered(
					ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
					ImGuiHoveredFlags_ChildWindows);
				HelperTheme::EndPad();
				if (!open)
				{
					G::ShowWatch = false;
					if (!G::ShowWatchMirror)
						WatchCapture::Stop();
					Settings::SetDirty();
				}
			}
			else
			{
				HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);
				ImGui::PushID("gw2igh_watch_pad");

				static const char* kTabs[] = { "Watch", "About" };
				gControlTab = PadNav::DrawTabs("###gw2igh_watch_ctrl_tabs", kTabs, 2, gControlTab);

				if (gControlTab == 1)
					WatchPadDetail::DrawHelp();
				else
					WatchPadDetail::DrawWatchControls();

				ImGui::PopID();
				if (PadDock::Capture(G::PadWatch))
					Settings::SetDirty();
				hovered |= ImGui::IsWindowHovered(
					ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
					ImGuiHoveredFlags_ChildWindows);
				HelperTheme::EndPad();
				if (!open)
				{
					G::ShowWatch = false;
					if (!G::ShowWatchMirror)
						WatchCapture::Stop();
					Settings::SetDirty();
				}
			}
			gRequestDock = false;
		}
		hovered |= RenderMirror();
	}
	catch (...)
	{
	}
	return hovered;
}
