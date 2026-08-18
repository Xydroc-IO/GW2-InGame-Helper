#include "WatchPadInternal.h"

#include "CrashTrail.h"
#include "EiRuntime.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "Settings.h"
#include "WatchCapture.h"
#include "WatchPad.h"
#include "WinePadOpen.h"
#include "WatchLinux.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <algorithm>
#include <cstdio>
#include <windows.h>

namespace WatchPadDetail
{
	int  gSelected = -1;
	char gFilter[64] = {};
	int  gDeferStartFrames = 0;
	int  gDeferStopFrames = 0;
	int  gDeferWatchOpenFrames = 0;
	int  gReopenGateFrames = 0;
	int  gPostStopCooldown = 0;
	int  gUploadHoldFrames = 0;
	int  gWatchOpenAge = 0;
	unsigned int gLastSoftStopMs = 0;
	unsigned int gMirrorSessionEndMs = 0;
	int  gSoftStopPhase = 0;
	int  gSoftStopFrames = 0;
	int  gSoftOpenDirtyFrames = 0;

	bool ReopenBlocked()
	{
		return gDeferStopFrames > 0 || gReopenGateFrames > 0 || gPostStopCooldown > 0
			|| gSoftStopPhase > 0
			|| WatchCapture::GpuParkBusy();
	}

	bool SoftOpenBlocked()
	{
		if (gDeferStopFrames > 0 || gSoftStopPhase > 0)
			return true;
		if (gReopenGateFrames > 0 || gPostStopCooldown > 0)
			return true;
		/* Soft-stop no longer parks GPU — do not wait GpuParkBusy (that blocked Soft-open
		   while dead SRVs from older paths aged out). */
		if (WatchCapture::IsCapturing() || WatchCapture::IsStreaming())
			return true;
		if (gLastSoftStopMs == 0)
			return false;
		const unsigned int now = GetTickCount();
		return (now - gLastSoftStopMs) < 3000u;
	}

	bool SoftStartBlocked()
	{
		/* Must NOT wait on IsCapturing — Soft Start sets it before the pump runs;
		   SoftOpenBlocked forever left the UI stuck on "Starting…". */
		if (gDeferStopFrames > 0 || gSoftStopPhase > 0 || WatchCapture::GpuParkBusy())
			return true;
		if (gReopenGateFrames > 0 || gPostStopCooldown > 0)
			return true;
		if (gLastSoftStopMs == 0)
			return false;
		const unsigned int now = GetTickCount();
		return (now - gLastSoftStopMs) < 2000u;
	}

	void MarkMirrorSessionEnded()
	{
		gMirrorSessionEndMs = GetTickCount();
	}

	bool CompanionSoftBlocked()
	{
		/* Only soft-stop drain — no extra 10s wall (Events open felt stuck). */
		return gDeferStopFrames > 0 || gSoftStopPhase > 0;
	}

	void ArmReopenGate()
	{
		const int gate = WinePadOpen::DeferFrames() + 18;
		gReopenGateFrames = std::max(gReopenGateFrames, gate);
		gPostStopCooldown = std::max(gPostStopCooldown, 45);
		gLastSoftStopMs = GetTickCount();
	}

	void QueueWineStop()
	{
		gDeferStartFrames = 0;
		gDeferMirrorOpenFrames = 0;
		gDeferWatchOpenFrames = 0;
		WinePadOpen::WatchSoftOpenFrames() = 0;
		gWantMirrorWhenReady = false;
		gUploadHoldFrames = 0;
		/* Stream usually SoftStopCapture'd mid Soft-stop — short settle only. */
		if (WatchCapture::IsCapturing() || WatchCapture::IsStreaming())
			gDeferStopFrames = std::max(WinePadOpen::DeferFrames() * 2, 8);
		else
			gDeferStopFrames = std::max(WinePadOpen::DeferFrames(), 3);
		ArmReopenGate();
	}

	void ArmWineSoftStop()
	{
		if (gSoftStopPhase > 0 || gDeferStopFrames > 0)
			return;
		CrashTrail::ArmDetail(48);
		CrashTrail::Mark("softstop");
		CrashTrail::NoteF("softstop:arm mirror=%d cap=%d stream=%d content=%d",
			G::ShowWatchMirror ? 1 : 0,
			WatchCapture::IsCapturing() ? 1 : 0,
			WatchCapture::IsStreaming() ? 1 : 0,
			WatchCapture::HasContent() ? 1 : 0);
		/* Do NOT CancelCompanionOpen — soft-open queues wait on Mirror 
		   stop ; clearing here forced a second rail click . */
		gDeferWatchOpenFrames = 0;
		WinePadOpen::WatchSoftOpenFrames() = 0;
		WinePadOpen::WatchSoftOpenFired() = false;
		gDeferStartFrames = 0;
		gDeferMirrorOpenFrames = 0;
		gWantMirrorWhenReady = false;
		/* Keep ShowWatchMirror — skipping Begin after a live stream tips Wine. */
		gMirrorInputBusy = true;
		CrashTrail::Note("softstop:pre HideContent");
		WatchCapture::HideContent();
		CrashTrail::Note("softstop:post HideContent");
		WinePadOpen::WatchMirrorQuietFrames() =
			std::max(WinePadOpen::WatchMirrorQuietFrames(),
				WinePadOpen::DeferFrames() * 2);
		gSoftStopPhase = 1;
		gSoftStopFrames = 0;
		gLastSoftStopMs = GetTickCount();
		CrashTrail::Note("softstop:phase1");
	}

	void TickSoftStopPhase()
	{
		if (gSoftStopPhase <= 0)
			return;
		gMirrorInputBusy = true;
		WatchCapture::HideContent();
		WinePadOpen::WatchMirrorQuietFrames() =
			std::max(WinePadOpen::WatchMirrorQuietFrames(), 2);
		++gSoftStopFrames;

		if (gSoftStopPhase == 1)
		{
			/* Phase 1: Mirror still Begins, no upload — tear stream ASAP off click. */
			G::ShowWatchMirror = true;
			if (gSoftStopFrames == 3
				&& (WatchCapture::IsCapturing() || WatchCapture::IsStreaming()))
			{
				CrashTrail::NoteF("softstop:pre SoftStopCapture f=%d", gSoftStopFrames);
				WatchCapture::SoftStopCapture();
				CrashTrail::NoteF("softstop:post SoftStopCapture cap=%d",
					WatchCapture::IsCapturing() ? 1 : 0);
				MarkMirrorSessionEnded();
			}
			if (gSoftStopFrames < 8)
				return;
			CrashTrail::Note("softstop:hide_mirror");
			G::ShowWatchMirror = false;
			gWantMirrorWhenReady = false;
			gDeferMirrorOpenFrames = 0;
			gSoftStopPhase = 2;
			gSoftStopFrames = 0;
			CrashTrail::Note("softstop:phase2");
			return;
		}

		/* Phase 2: Mirror window gone — brief settle before defer Stop. */
		if (gSoftStopFrames < 5)
			return;
		CrashTrail::Note("softstop:pre QueueWineStop");
		gSoftStopPhase = 0;
		gSoftStopFrames = 0;
		QueueWineStop();
		CrashTrail::NoteF("softstop:done deferStop=%d", gDeferStopFrames);
		CrashTrail::ArmDetail(gDeferStopFrames + 12);
	}

	void TickDeferredWatchOpen()
	{
		TickSoftStopPhase();
		if (CrashTrail::DetailArmed() && gDeferStopFrames > 0)
			CrashTrail::NoteF("softstop:tick_after defer=%d showWatch=%d",
				gDeferStopFrames, G::ShowWatch ? 1 : 0);

		WinePadOpen::WatchSoftOpenFired() = false;
		if (WinePadOpen::WatchMirrorQuietFrames() > 0)
			--WinePadOpen::WatchMirrorQuietFrames();
		if (gReopenGateFrames > 0)
			--gReopenGateFrames;
		if (gPostStopCooldown > 0 && gDeferStopFrames <= 0 && !WatchCapture::GpuParkBusy())
			--gPostStopCooldown;
		if (gUploadHoldFrames > 0)
			--gUploadHoldFrames;
		/* One Save well after Soft-open Begin — never every frame while Watch is up. */
		if (gSoftOpenDirtyFrames > 0)
		{
			--gSoftOpenDirtyFrames;
			if (gSoftOpenDirtyFrames == 0 && !SoftOpenBlocked() && !ReopenBlocked())
				Settings::SetDirty();
		}

		if (G::ShowWatch)
			++gWatchOpenAge;
		else
			gWatchOpenAge = 0;

		/* Soft Start asked for pump without CreateThread on Soft frame. */
		if (WatchLinux::Available() && !SoftStartBlocked() && WatchLinux::ConsumeNeedPump())
		{
			WatchLinux::EnsurePumpNow();
			gDeferStartFrames = std::max(gDeferStartFrames, WinePadOpen::DeferFrames());
		}

		if (gDeferWatchOpenFrames <= 0)
		{
			WinePadOpen::WatchSoftOpenFrames() = 0;
			return;
		}
		/* Soft-stop still draining — do not Begin Watch. */
		if (gDeferStopFrames > 0 || gSoftStopPhase > 0)
		{
			gDeferWatchOpenFrames = 0;
			WinePadOpen::WatchSoftOpenFrames() = 0;
			return;
		}
		--gDeferWatchOpenFrames;
		WinePadOpen::WatchSoftOpenFrames() = gDeferWatchOpenFrames;
		if (gDeferWatchOpenFrames > 0)
			return;
		if (SoftOpenBlocked())
		{
			gDeferWatchOpenFrames = WinePadOpen::DeferFrames();
			WinePadOpen::WatchSoftOpenFrames() = gDeferWatchOpenFrames;
			return;
		}
		WinePadOpen::WatchSoftOpenFired() = false;
		WinePadOpen::WatchSoftOpenFrames() = 0;
		gMirrorInputBusy = false;
		WatchPad::Open();
		/* Persist ShowWatch after Soft Begin settles — not on click / Begin frame. */
		gSoftOpenDirtyFrames = 45;
	}

	void TickDeferredStartStop()
	{
		if (gDeferStopFrames > 0)
		{
			CrashTrail::NoteF("softstop:defer f=%d", gDeferStopFrames);
			--gDeferStopFrames;
			if (gDeferStopFrames == 0)
			{
				CrashTrail::Note("softstop:defer_fire");
				/* Wine Soft-stop: keep D3D textures — Release/park tipped Soft-open.
				   SoftStopCapture often already ran mid Soft-stop (tear stream early). */
				if (EiRuntime::IsWine())
				{
					if (WatchCapture::IsCapturing() || WatchCapture::IsStreaming())
					{
						CrashTrail::Note("softstop:capture");
						WatchCapture::SoftStopCapture();
					}
					else
						CrashTrail::Note("softstop:defer_fire idle");
				}
				else
					WatchCapture::Stop();
				if (gMirrorSessionEndMs == 0)
					MarkMirrorSessionEnded();
				G::ShowWatchMirror = false;
				gWantMirrorWhenReady = false;
				gDeferMirrorOpenFrames = 0;
				CrashTrail::Note("softstop:pre ArmReopenGate");
				ArmReopenGate();
				CrashTrail::Note("softstop:pre PollJoinStart");
				WatchLinux::PollJoinStart();
				CrashTrail::NoteF("softstop:defer_done cool=%d gate=%d",
					gPostStopCooldown, gReopenGateFrames);
				CrashTrail::ArmDetail(gPostStopCooldown + 8);
				if (!EiRuntime::IsWine())
					Settings::SetDirty();
			}
		}
		if (gDeferStartFrames > 0)
		{
			--gDeferStartFrames;
			if (gDeferStartFrames == 0)
			{
				if (!G::ShowWatch && !G::ShowWatchMirror)
					return;
				if (G::ShowWatch && gWatchOpenAge < 10)
				{
					gDeferStartFrames = 10 - gWatchOpenAge;
					return;
				}
				if (SoftStartBlocked())
				{
					gDeferStartFrames = WinePadOpen::DeferFrames();
					return;
				}
				WatchCapture::Start(0);
				gUploadHoldFrames = std::max(gUploadHoldFrames, 20);
				RequestMirrorWhenReady();
			}
		}
	}

	void EnsureList()
	{
		if (EiRuntime::IsWine())
			return;
		/* Never auto-EnumWindows here — that runs during ImGui and has asserted on
		   native Windows after close/reopen. User hits Refresh (or we leave empty). */
	}

	void OpenMirror()
	{
		if (G::ShowWatchMirror || gDeferMirrorOpenFrames > 0)
			return;
		/* Wine: open Mirror on a later frame than first-frame detect — Begin+upload
		   same frame as Soft work has tipped Wine. */
		if (EiRuntime::IsWine())
		{
			int defer = WinePadOpen::DeferFrames();
			if (gPostStopCooldown > 0 || gReopenGateFrames > 0)
				defer = std::max(defer * 3, 15);
			gDeferMirrorOpenFrames = defer;
			WinePadOpen::WatchMirrorQuietFrames() =
				std::max(WinePadOpen::WatchMirrorQuietFrames(), defer);
			/* No SetDirty on Wine Soft Mirror open — Save raced Soft Begin. */
			return;
		}
		G::ShowWatchMirror = true;
		gRequestMirrorDock = true;
		if (G::PadWatchMirror.w < 80.f || G::PadWatchMirror.h < 80.f)
		{
			G::PadWatchMirror.w = 0.f;
			G::PadWatchMirror.h = 0.f;
		}
		PadDock::ClearCustomCollapsed("Watch Mirror###GW2InGameHelperWatchMirror");
		Settings::SetDirty();
	}

	void RequestMirrorWhenReady()
	{
		/* Do not Begin Mirror until the first frame is uploaded — portal/picker
		   used to open an empty Mirror and tip Wine under load. */
		gWantMirrorWhenReady = true;
		if (!EiRuntime::IsWine())
			Settings::SetDirty();
	}

	void TickMirrorWhenReady()
	{
		if (!gWantMirrorWhenReady)
			return;
		if (G::ShowWatchMirror)
		{
			gWantMirrorWhenReady = false;
			return;
		}
		/* Soft defer in flight — keep WantMirror so Tick still uploads. */
		if (gDeferMirrorOpenFrames > 0)
			return;
		if (!WatchCapture::IsCapturing())
		{
			gWantMirrorWhenReady = false;
			return;
		}
		/* Wait out GPU park from a prior Stop — reopen Begin+Create tipped Wine. */
		if (WatchCapture::GpuParkBusy() || WatchPadDetail::gReopenGateFrames > 0
			|| (WatchLinux::Available() && WatchLinux::IsStarting()))
			return;
		/* IsStreaming = capturing + real uploaded pixels (not portal wait). */
		if (!WatchCapture::IsStreaming())
			return;
		OpenMirror();
		if (G::ShowWatchMirror)
			gWantMirrorWhenReady = false;
	}

	void TickDeferredMirrorOpen()
	{
		if (gDeferMirrorOpenFrames <= 0)
			return;
		--gDeferMirrorOpenFrames;
		if (gDeferMirrorOpenFrames > 0 || G::ShowWatchMirror)
			return;
		if (WatchCapture::GpuParkBusy() || gReopenGateFrames > 0)
		{
			gDeferMirrorOpenFrames = 1;
			return;
		}
		/* Start cancelled / Watch closed before Mirror defer landed. */
		if (!WatchCapture::IsStreaming())
		{
			gWantMirrorWhenReady = false;
			return;
		}
		/* Quiet uploads for a few Begins after Mirror appears (reopen crash). */
		if (EiRuntime::IsWine())
			WinePadOpen::WatchMirrorQuietFrames() =
				std::max(WinePadOpen::WatchMirrorQuietFrames(), 3);
		G::ShowWatchMirror = true;
		gWantMirrorWhenReady = false;
		gRequestMirrorDock = true;
		if (G::PadWatchMirror.w < 80.f || G::PadWatchMirror.h < 80.f)
		{
			G::PadWatchMirror.w = 0.f;
			G::PadWatchMirror.h = 0.f;
		}
		PadDock::ClearCustomCollapsed("Watch Mirror###GW2InGameHelperWatchMirror");
		if (!EiRuntime::IsWine())
			Settings::SetDirty();
	}

	void QueueWineStart()
	{
		/* Never cancel a pending Stop — that left capture live and Start→Mirror
		   reopen tipped Wine. Start waits out stop/park in TickDeferredStartStop. */
		if (gDeferStopFrames <= 0 &&
			(WatchCapture::IsCapturing() || WatchCapture::IsStreaming()))
		{
			G::ShowWatchMirror = false;
			gWantMirrorWhenReady = false;
			gDeferMirrorOpenFrames = 0;
			gDeferStopFrames = WinePadOpen::DeferFrames();
			ArmReopenGate();
		}
		gDeferStartFrames = WinePadOpen::DeferFrames();
		if (ReopenBlocked())
			gDeferStartFrames += std::max(gDeferStopFrames, gReopenGateFrames) + WinePadOpen::DeferFrames();
		if (!EiRuntime::IsWine())
			Settings::SetDirty();
	}
}
