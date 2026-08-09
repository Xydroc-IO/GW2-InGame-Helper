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
#include "CrashTrail.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <d3d11.h>
#include <windows.h>

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
		{
			WatchPadDetail::gMirrorInputBusy = false;
			return false;
		}
		/* soft-open settle / fire : skip Mirror Begin briefly . 
		   Do NOT skip while merely queued (CompanionWaitingOnMirror ) —
		   that made every rail click look like it closed Mirror. */
		static bool sLoggedSkipBegin = false;
		const bool settleBusy = EiRuntime::IsWine()
			&& (WinePadOpen::CompanionFiredThisFrame()
				|| WinePadOpen::CompanionSettleFrames() > 0);
		if (settleBusy)
		{
			WatchPadDetail::gMirrorInputBusy = true;
			if (!sLoggedSkipBegin)
			{
				CrashTrail::Note("mirror:skip_begin settle");
				sLoggedSkipBegin = true;
			}
			return false;
		}
		sLoggedSkipBegin = false;
		/* Quiet GPU while soft-open / soft-stop — keep Begin otherwise . */
		const bool quietUpload = WinePadOpen::SoftWorkBusy()
			|| WinePadOpen::WatchMirrorQuietFrames() > 0
			|| WinePadOpen::WatchSoftOpenFired()
			|| WatchPadDetail::gSoftStopPhase > 0;

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

		auto closeMirror = [&]() {
			if (EiRuntime::IsWine())
			{
				/* Soft-stop keeps Begin while quieting — never clear flag on X click. */
				WatchPadDetail::ArmWineSoftStop();
				return;
			}
			G::ShowWatchMirror = false;
			WatchPadDetail::gWantMirrorWhenReady = false;
			WatchPadDetail::gDeferMirrorOpenFrames = 0;
			WatchPadDetail::gMirrorInputBusy = true;
			WatchCapture::HideContent();
			WatchCapture::Stop();
			Settings::SetDirty();
		};

		if (!theme.AfterBegin("Watch Mirror", &open) || !padBody)
		{
			if (PadDock::Capture(G::PadWatchMirror) && !EiRuntime::IsWine())
				Settings::SetDirty();
			const bool hovered = ImGui::IsWindowHovered(
				ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
				ImGuiHoveredFlags_ChildWindows);
			WatchPadDetail::gMirrorInputBusy = quietUpload || (hovered &&
				(ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
					ImGui::IsMouseDragging(ImGuiMouseButton_Left)));
			HelperTheme::EndPad();
			if (!open)
				closeMirror();
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
		PaintStageFrame(dl, stage0, stage1, capturing && !quietUpload);

		const uint32_t cw = WatchCapture::ContentW();
		const uint32_t ch = WatchCapture::ContentH();
		ID3D11ShaderResourceView* srv = WatchCapture::Srv();
		const ImVec2 inner0(stage0.x + kPad, stage0.y + kPad);
		const ImVec2 inner1(stage1.x - kPad, stage1.y - kPad);
		const float innerW = inner1.x - inner0.x;
		const float innerH = inner1.y - inner0.y;
		if (!quietUpload && srv && cw > 0 && ch > 0 && innerW > 8.f && innerH > 8.f)
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
				const float u1 = WatchCapture::ContentU();
				const float v1 = WatchCapture::ContentV();
				dl->AddImage(reinterpret_cast<ImTextureID>(srv), img0, img1,
					ImVec2(0.f, 0.f), ImVec2(u1, v1));
				dl->AddRect(img0, img1, ColU32(HelperTheme::GoldDim, 0.35f * G::Opacity), 0.f, 0, 1.f);
			}
		}
		else
		{
			ImGui::SetCursorScreenPos(ImVec2(inner0.x + kInset, inner0.y + kInset));
			ImGui::PushTextWrapPos(inner1.x - kInset);
			if (quietUpload && capturing)
				ImGui::TextColored(HelperTheme::Muted, "Mirror paused…");
			else
				ImGui::TextColored(HelperTheme::Muted, "%s",
					capturing
						? (EiRuntime::IsWine()
							? "Waiting for portal / first frame…"
							: "Waiting for first frame…")
						: "Stopped — use Start on the Watch pad.");
			ImGui::PopTextWrapPos();
		}

		ImGui::PopID();
		if (PadDock::Capture(G::PadWatchMirror) && !EiRuntime::IsWine())
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
			ImGuiHoveredFlags_ChildWindows);
		WatchPadDetail::gMirrorInputBusy = quietUpload || (hovered &&
			(ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
				ImGui::IsMouseDragging(ImGuiMouseButton_Left)));
		HelperTheme::EndPad();
		WatchPadDetail::gRequestMirrorDock = false;
		if (!open)
			closeMirror();
		return hovered;
	}
}

namespace WatchPadDetail
{
	bool gRequestMirrorDock = false;
	bool gWantMirrorWhenReady = false;
	bool gMirrorInputBusy = false;
	int  gDeferMirrorOpenFrames = 0;
}

void WatchPad::Open()
{
	G::ShowWatch = true;
	gRequestDock = true;
	gControlTab = 0;
	/* Do not EnumWindows/GetWindowText on the ImGui click frame — that has
	   asserted Size > 0 on native Windows. List refreshes via EnsureList / Refresh. */
	/* Wine Soft-open after Soft-stop: never Stop() here — capture may still be
	   draining (deferStop/phase). Stop on Open was the reopen tip (Soft-stop
	   survived; Soft-open Begin+Stop did not). Orphan Soft-stop owns Stop. */
	if (!EiRuntime::IsWine() && !G::ShowWatchMirror && WatchCapture::IsCapturing()
		&& WatchPadDetail::gDeferStopFrames <= 0 && WatchPadDetail::gSoftStopPhase <= 0)
		WatchCapture::Stop();
	if (!WatchCapture::IsStreaming())
		WatchPadDetail::gSelected = -1;
	if (G::PadWatch.w < 80.f || G::PadWatch.h < 80.f)
	{
		G::PadWatch.w = 0.f;
		G::PadWatch.h = 0.f;
	}
	if (ImGuiWindow* w = ImGui::FindWindowByName("Watch###GW2InGameHelperWatch"))
		w->StateStorage.SetBool(w->GetID("##gw2igh_pad_collapsed"), false);
	/* Wine Soft-open: defer SetDirty — Save on Open frame after Soft-stop tipped. */
	if (!EiRuntime::IsWine())
		Settings::SetDirty();
}

void WatchPad::CloseAll()
{
	WinePadOpen::CancelCompanionOpen();
	G::ShowWatch = false;
	G::ShowWatchMirror = false;
	WatchPadDetail::gWantMirrorWhenReady = false;
	WatchPadDetail::gMirrorInputBusy = false;
	WatchPadDetail::gDeferMirrorOpenFrames = 0;
	WatchPadDetail::gDeferStartFrames = 0;
	WatchPadDetail::gDeferStopFrames = 0;
	WatchPadDetail::gDeferWatchOpenFrames = 0;
	WatchPadDetail::gReopenGateFrames = 0;
	WatchPadDetail::gPostStopCooldown = 0;
	WatchPadDetail::gUploadHoldFrames = 0;
	WatchPadDetail::gWatchOpenAge = 0;
	WatchPadDetail::gLastSoftStopMs = 0;
	WatchPadDetail::gMirrorSessionEndMs = 0;
	WatchPadDetail::gSoftStopPhase = 0;
	WatchPadDetail::gSoftStopFrames = 0;
	WatchPadDetail::gSoftOpenDirtyFrames = 0;
	WinePadOpen::WatchSoftOpenFrames() = 0;
	WinePadOpen::WatchSoftOpenFired() = false;
	WinePadOpen::WatchMirrorQuietFrames() = 0;
	WatchCapture::Stop();
	Settings::SetDirty();
}

void WatchPad::ToggleControl()
{
	if (WatchPadDetail::gSoftStopPhase > 0)
		return; /* Soft-stop draining — ignore side-nav until done */

	if (G::ShowWatch)
	{
		/* Close control only — leave Mirror alone. No SetDirty on Wine click
		   while streaming (Save on Soft-stop tipped after multi-minute runs). */
		WinePadOpen::CancelCompanionOpen();
		G::ShowWatch = false;
		WatchPadDetail::gDeferWatchOpenFrames = 0;
		WinePadOpen::WatchSoftOpenFrames() = 0;
		WinePadOpen::WatchSoftOpenFired() = false;
		WatchPadDetail::gDeferStartFrames = 0;
		if (!G::ShowWatchMirror)
		{
			WatchPadDetail::gDeferMirrorOpenFrames = 0;
			WatchPadDetail::gWantMirrorWhenReady = false;
			if (WatchPadDetail::gDeferStopFrames <= 0)
				WatchCapture::Stop();
		}
		if (!(G::ShowWatchMirror || WatchCapture::IsStreaming()) && !EiRuntime::IsWine())
			Settings::SetDirty();
		return;
	}

	/* Live Mirror UI → Wine Soft-stop (keep Begin while quieting). */
	if (G::ShowWatchMirror || WatchPadDetail::gWantMirrorWhenReady
		|| WatchPadDetail::gDeferMirrorOpenFrames > 0)
	{
		if (EiRuntime::IsWine())
			WatchPadDetail::ArmWineSoftStop();
		else
		{
			WatchPadDetail::gDeferWatchOpenFrames = 0;
			WinePadOpen::WatchSoftOpenFrames() = 0;
			WinePadOpen::WatchSoftOpenFired() = false;
			WatchPadDetail::gDeferStartFrames = 0;
			WatchPadDetail::gDeferMirrorOpenFrames = 0;
			WatchPadDetail::gWantMirrorWhenReady = false;
			G::ShowWatchMirror = false;
			WatchPadDetail::gMirrorInputBusy = true;
			WatchCapture::HideContent();
			WatchCapture::Stop();
			Settings::SetDirty();
		}
		return;
	}
	if ((WatchCapture::IsCapturing() || WatchCapture::IsStreaming())
		&& WatchPadDetail::gDeferStopFrames <= 0)
	{
		WatchCapture::HideContent();
		if (EiRuntime::IsWine())
			WatchPadDetail::QueueWineStop();
		else
			WatchCapture::Stop();
		return;
	}

	/* Soft-open Start/Stop control when safe. */
	if (WinePadOpen::Soft())
	{
		int frames = WinePadOpen::DeferFrames();
		if (WatchPadDetail::SoftOpenBlocked())
			frames += 90;
		WatchPadDetail::gDeferWatchOpenFrames = frames;
		WinePadOpen::WatchSoftOpenFrames() = frames;
		return;
	}
	WatchPad::Open();
}

bool WatchPad::Render()
{
	bool hovered = false;
	try
	{
		WatchPadDetail::TickDeferredStartStop();
		WatchPadDetail::TickDeferredMirrorOpen();
		WatchPadDetail::TickDeferredWatchOpen();

		/* Orphan capture: no UI and no Soft Start/Mirror/open pending — stop watchd. */
		if (!G::ShowWatch && !G::ShowWatchMirror &&
			!WatchPadDetail::gWantMirrorWhenReady &&
			WatchPadDetail::gDeferStartFrames <= 0 &&
			WatchPadDetail::gDeferMirrorOpenFrames <= 0 &&
			WatchPadDetail::gDeferStopFrames <= 0 &&
			WatchPadDetail::gDeferWatchOpenFrames <= 0 &&
			WatchPadDetail::gSoftStopPhase <= 0 &&
			WatchCapture::IsCapturing())
			WatchCapture::Stop();

		/* Mirror opens via RequestMirrorWhenReady after first uploaded frame. */

		if (G::ShowWatch)
		{
			/* Wine soft-stop drain : skip Watch control Begin 
			   —(Events / etc already drew ; trail died 
			   right after softstop : done ) . */
			if (EiRuntime::IsWine()
				&& (WatchPadDetail::gDeferStopFrames > 0
					|| WatchPadDetail::gSoftStopPhase > 0))
			{
				CrashTrail::NoteF("watch:skip Begin softstop defer=%d phase=%d",
					WatchPadDetail::gDeferStopFrames, WatchPadDetail::gSoftStopPhase);
			}
			else
			{
			constexpr float kPadW = 440.f;
			constexpr float kPadH = 320.f;

			if (CrashTrail::DetailArmed())
				CrashTrail::Note("watch:pre Begin");
			PadDock::SetSizeConstraints("Watch###GW2InGameHelperWatch", 320.f, 200.f,
				PadDock::MaxW(640.f), PadDock::MaxH(640.f));
			ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
			PadDock::Place(G::PadWatch, gRequestDock, kPadW, kPadH, PadDock::BesideHelper(kPadW));
			if (!gRequestDock && (G::PadWatch.w < 80.f || G::PadWatch.h < 80.f))
				ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_Always);

			bool open = G::ShowWatch;
			HelperTheme::ScopedWindow theme(G::Opacity);
			const bool padBody = ImGui::Begin("Watch###GW2InGameHelperWatch", &open, HelperTheme::PadFlags());
			if (CrashTrail::DetailArmed())
				CrashTrail::NoteF("watch:post Begin body=%d", padBody ? 1 : 0);
			if (!theme.AfterBegin("Watch", &open) || !padBody)
			{
				if (PadDock::Capture(G::PadWatch) && !EiRuntime::IsWine())
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
					if (!EiRuntime::IsWine())
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
				if (PadDock::Capture(G::PadWatch) && !EiRuntime::IsWine())
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
					if (!EiRuntime::IsWine())
						Settings::SetDirty();
				}
			}
			gRequestDock = false;
			/* Wine Soft-open: one-shot dirty via gSoftOpenDirtyFrames — never every frame. */
			} /* else: not soft-stop skipping Begin */
		}
		if (CrashTrail::DetailArmed())
			CrashTrail::Note("watch:pre RenderMirror");
		hovered |= RenderMirror();
		if (CrashTrail::DetailArmed())
			CrashTrail::Note("watch:post RenderMirror");

		/* Tick after Mirror Begin so click/drag sets gMirrorInputBusy before
		   UpdateSubresource — same-frame upload+interaction tipped Wine. */
		if (CrashTrail::DetailArmed())
			CrashTrail::Note("watch:pre Capture::Tick");
		WatchCapture::Tick();
		if (CrashTrail::DetailArmed())
			CrashTrail::Note("watch:post Capture::Tick");
		if (CrashTrail::DetailArmed())
			CrashTrail::Note("watch:pre TickMirrorWhenReady");
		WatchPadDetail::TickMirrorWhenReady();
		if (CrashTrail::DetailArmed())
			CrashTrail::Note("watch:render_end");
	}
	catch (...)
	{
	}
	return hovered;
}
