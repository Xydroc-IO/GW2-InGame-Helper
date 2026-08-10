#pragma once

/* Wine often kills GW2 with no useful dump. Flight-recorder breadcrumbs + a
   light exception filter write under AddonPaths::CrashLogsDir() (Crash-Logs/):
   - crash-trail.txt                 — live ring (last N notes; flushed often)
   - crash.log                       — short append index (one line per snapshot)
   - YYYY-MM-DD_HH-MM-SS_mmm/        — one folder per tip / SEH / orphan promote
       snapshot.txt                  — rich dump
       crash-trail.txt               — trail copy at snapshot time */
namespace CrashTrail
{
	void Install();
	void Shutdown();

	/* tag: short ASCII, e.g. "softopen:Events mirror_hot=1" */
	void Note(const char* tag);

	/* printf-style note (truncated). */
	void NoteF(const char* fmt, ...);

	/* Sticky breadcrumb always echoed in crash-0 (survives ring wrap). */
	void Mark(const char* tag);

	/* Nexus / Present phase — updates sticky only (no ring flood). crash-0 shows it. */
	void SetPhase(const char* phase);
	const char* Phase();

	/* True while soft-open settle / soft-stop / armed detail 
	   window — use to gate noisy probes. */
	bool DetailArmed();

	/* Arm DetailArmed for N frames (call from softfire / softstop / risky paths). */
	void ArmDetail(int frames);

	/* Decrement detail window; call once per UI frame. */
	void Tick();

	/* Periodic note while Mirror + any companion pad are up (catches delayed tips). */
	void HeartbeatIfHot();

	/* Force-write crash-trail.txt now. */
	void Flush();

	/* RAII enter/leave notes when DetailArmed (destructor still runs on C++ unwind;
	   hard Wine tips skip the leave tag — sticky stays on enter). */
	struct Scope
	{
		explicit Scope(const char* enter, const char* leave = nullptr);
		~Scope();
		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;
	private:
		char leave_[96]{};
		bool on_ = false;
	};
}
