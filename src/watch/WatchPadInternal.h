#pragma once

#include <cstdint>

/* Shared helpers for WatchPad*.cpp (not public API). */
namespace WatchPadDetail
{
	void DrawHelp();
	void DrawWatchControls();
	void OpenMirror();
	void RequestMirrorWhenReady(); /* open Mirror after first uploaded frame */
	void TickMirrorWhenReady();
	void TickDeferredMirrorOpen();
	void QueueWineStart(); /* Start off the click frame; Mirror waits for frames */
	void QueueWineStop();  /* Stop off Mirror-close / Stop-button click */
	void TickDeferredStartStop();
	void TickDeferredWatchOpen(); /* Wine Soft Begin — dedicated, not SoftWorkBusy */
	void EnsureList();

	extern bool gRequestMirrorDock;
	extern bool gWantMirrorWhenReady;
	extern bool gMirrorInputBusy; /* prior-frame drag/click on live Mirror */
	extern int  gDeferMirrorOpenFrames;
	extern int  gDeferStartFrames;
	extern int  gDeferStopFrames;
	extern int  gDeferWatchOpenFrames; /* Wine Soft pad Begin — not SoftWorkBusy */
	extern int  gReopenGateFrames; /* after Soft-stop — block Soft-open/Start until park settles */
	extern int  gPostStopCooldown; /* extra frames after Stop before Soft-open/Start */
	extern int  gUploadHoldFrames; /* skip GPU upload after Soft Start */
	extern int  gWatchOpenAge; /* frames Watch pad has been open */
	extern unsigned int gLastSoftStopMs; /* GetTickCount — Soft-open waits wall clock */
	extern unsigned int gMirrorSessionEndMs; /* SoftStopCapture done — companion Soft Begin waits longer */
	extern int  gSoftStopPhase; /* Wine: 0 idle, 1 quiet Begin, 2 hidden settle, then QueueWineStop */
	extern int  gSoftStopFrames;
	extern int  gSoftOpenDirtyFrames; /* Wine: one-shot SetDirty after Soft-open settles */
	extern int  gSelected;
	extern char gFilter[64];

	bool ReopenBlocked(); /* stop/park/gate/cooldown */
	bool SoftOpenBlocked(); /* Soft-open Watch Begin — wait for idle after Soft-stop */
	bool SoftStartBlocked(); /* Soft Start — must not wait on IsCapturing (Start sets it) */
	bool CompanionSoftBlocked(); /* open Events/Account — soft-stop drain only */
	void ArmReopenGate(); /* after Soft-stop — block Soft-open/Start until park settles */
	void ArmWineSoftStop(); /* Soft-stop Mirror/stream off side-nav / X — keep Begin while quieting */
	void TickSoftStopPhase();
	void MarkMirrorSessionEnded(); /* SoftStopCapture / hard Stop */
}
