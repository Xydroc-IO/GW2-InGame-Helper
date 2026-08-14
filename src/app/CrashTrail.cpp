#include "CrashTrail.h"
#include "CrashTrailInternal.h"

#include "AddonVersion.h"
#include "EiRuntime.h"
#include "Globals.h"
#include "WatchCapture.h"
#include "WatchPadInternal.h"
#include "WinePadOpen.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <windows.h>

using namespace CrashTrailDetail;

namespace CrashTrailDetail
{
CRITICAL_SECTION gCs{};
bool             gCsReady = false;
bool             gInstalled = false;
Entry            gRing[kRing]{};
int              gHead = 0;
int              gCount = 0;
int              gNotesSinceFlush = 0;
PVOID            gVectored = nullptr;
LPTOP_LEVEL_EXCEPTION_FILTER gPrevFilter = nullptr;
volatile LONG    gCrashSnapDone = 0;
volatile LONG    gInCrashSnapshot = 0;
char             gStickyMark[kTagMax]{};
DWORD            gStickyMarkTick = 0;
char             gPhase[64] = "idle";
DWORD            gPhaseTick = 0;
int              gDetailFrames = 0;
unsigned         gNoteSeq = 0;
DWORD            gLastFlushMs = 0;
char             gHbStatus[192]{};
DWORD            gHbTick = 0;

void EnsureCs()
{
	if (gCsReady)
		return;
	InitializeCriticalSection(&gCs);
	gCsReady = true;
}

/* Proton Experimental ntdll!call_tls_callbacks (RVA 0x47838) does:
     AddressOfCallBacks = *(tls_dir + 0x18);  then  mov rax, [rax]
   A poisoned AddressOfCallBacks of -1 passes the null check and AVs on read
   (sticky RT_PostRender while Wine runs THREAD_ATTACH during Present).

   Pin callbacks + a full TLS directory in RW static storage and retarget the
   PE DataDirectory so Wine never walks .rdata even if VirtualProtect fails. */
extern "C" void NTAPI __dyn_tls_init(PVOID, DWORD, PVOID);
extern "C" void NTAPI __dyn_tls_dtor(PVOID, DWORD, PVOID);

void HardenSelfTlsCallbacks()
{
	HMODULE self = nullptr;
	if (!GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
				GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&HardenSelfTlsCallbacks),
			&self) ||
		!self)
		return;

	auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(self);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		return;
	auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(
		reinterpret_cast<BYTE*>(self) + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE)
		return;
	IMAGE_DATA_DIRECTORY& dd =
		nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
	if (dd.VirtualAddress == 0 || dd.Size < sizeof(IMAGE_TLS_DIRECTORY64))
		return;

	auto* dir = reinterpret_cast<IMAGE_TLS_DIRECTORY64*>(
		reinterpret_cast<BYTE*>(self) + dd.VirtualAddress);
	const auto base = reinterpret_cast<ULONG_PTR>(self);
	const auto end = base + nt->OptionalHeader.SizeOfImage;
	auto inMod = [base, end](ULONG_PTR p) {
		return p >= base && p < end;
	};
	auto readable = [](const void* p, size_t n) {
		if (!p || n == 0)
			return false;
		MEMORY_BASIC_INFORMATION mbi{};
		const auto* bytes = static_cast<const BYTE*>(p);
		size_t left = n;
		while (left > 0)
		{
			if (VirtualQuery(bytes, &mbi, sizeof(mbi)) == 0)
				return false;
			if (mbi.State != MEM_COMMIT)
				return false;
			const DWORD prot = mbi.Protect & 0xffu;
			if (prot == PAGE_NOACCESS || prot == PAGE_EXECUTE ||
				(mbi.Protect & PAGE_GUARD))
				return false;
			const auto regionEnd = reinterpret_cast<const BYTE*>(mbi.BaseAddress) + mbi.RegionSize;
			const size_t chunk = static_cast<size_t>(regionEnd - bytes);
			const size_t step = chunk < left ? chunk : left;
			bytes += step;
			left -= step;
		}
		return true;
	};

	static PIMAGE_TLS_CALLBACK sSafe[8]{};
	static IMAGE_TLS_DIRECTORY64 sDir{};
	static bool sDone = false;
	if (sDone)
		return;

	const ULONG_PTR rawCbs = static_cast<ULONG_PTR>(dir->AddressOfCallBacks);
	const bool cbsPoison = rawCbs == 0 || rawCbs == static_cast<ULONG_PTR>(-1) ||
		!inMod(rawCbs);

	PIMAGE_TLS_CALLBACK* src = nullptr;
	if (!cbsPoison && readable(reinterpret_cast<const void*>(rawCbs), sizeof(void*)))
		src = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(rawCbs);

	int n = 0;
	if (src)
	{
		for (; n < 7; ++n)
		{
			if (!readable(&src[n], sizeof(src[n])))
			{
				n = 0;
				break;
			}
			const PIMAGE_TLS_CALLBACK cb = src[n];
			if (!cb)
				break;
			if (!inMod(reinterpret_cast<ULONG_PTR>(cb)))
			{
				n = 0;
				break;
			}
			sSafe[n] = cb;
		}
	}
	if (n == 0)
	{
		sSafe[0] = __dyn_tls_init;
		sSafe[1] = __dyn_tls_dtor;
		n = 2;
	}
	sSafe[n] = nullptr;

	sDir = *dir;
	sDir.AddressOfCallBacks = reinterpret_cast<ULONG_PTR>(sSafe);

	const auto dirRva = static_cast<DWORD>(
		reinterpret_cast<ULONG_PTR>(&sDir) - base);
	int patched = 0;

	DWORD oldProt = 0;
	if (VirtualProtect(&dd, sizeof(dd), PAGE_READWRITE, &oldProt))
	{
		dd.VirtualAddress = dirRva;
		dd.Size = sizeof(sDir);
		DWORD ignore = 0;
		VirtualProtect(&dd, sizeof(dd), oldProt, &ignore);
		++patched;
	}

	/* Best-effort in-place fix when .rdata is writable on this Wine. */
	if (VirtualProtect(dir, sizeof(*dir), PAGE_READWRITE, &oldProt))
	{
		dir->AddressOfCallBacks = reinterpret_cast<ULONG_PTR>(sSafe);
		DWORD ignore = 0;
		VirtualProtect(dir, sizeof(*dir), PAGE_READONLY, &ignore);
		++patched;
	}

	sDone = patched > 0;
	CrashTrail::NoteF("tls_harden poison=%d cbs=%d patched=%d dirRva=0x%X",
		cbsPoison ? 1 : 0, n, patched, static_cast<unsigned>(dirRva));
}

bool CriticalFlushTag(const char* tag)
{
	if (!tag)
		return false;
	return std::strstr(tag, "softopen")
		|| std::strstr(tag, "softfire")
		|| std::strstr(tag, "softstop")
		|| std::strstr(tag, "mark:")
		|| std::strstr(tag, "save:")
		|| std::strstr(tag, "install")
		|| std::strstr(tag, "shutdown")
		|| std::strstr(tag, "orphan");
}

bool AnyCompanionPadOpen()
{
	return G::ShowNotes || G::ShowAccount || G::ShowTpWatch || G::ShowLookup
		|| G::ShowWallet || G::ShowVault || G::ShowEvents || G::ShowLogManager
		|| G::ShowEconomy || G::ShowCrafting || G::ShowInstances || G::ShowCompletion || G::ShowAchievements || G::ShowFarming
		|| G::ShowPathingGuides || G::ShowCompassPad
		|| G::ShowWatch || G::ShowWatchMirror || G::ShowSettings;
}

void WriteTrailToFile(FILE* f)
{
	if (!f)
		return;
	const int n = gCount < kRing ? gCount : kRing;
	const int start = (gHead - n + kRing) % kRing;
	DWORD prevTick = 0;
	bool havePrev = false;
	for (int i = 0; i < n; ++i)
	{
		const Entry& e = gRing[(start + i) % kRing];
		const unsigned long gap = havePrev
			? static_cast<unsigned long>(e.tick - prevTick)
			: 0ul;
		std::fprintf(f, "+%lums  tick=%lu  %s\n",
			gap, static_cast<unsigned long>(e.tick), e.tag);
		prevTick = e.tick;
		havePrev = true;
	}
}

void WriteTrailUnlocked()
{
	const std::wstring path = TrailPath();
	if (path.empty())
		return;
	FILE* f = _wfopen(path.c_str(), L"wb");
	if (!f)
		return;
	SYSTEMTIME st{};
	GetLocalTime(&st);
	std::fprintf(f, "GW2-InGame-Helper %d.%d.%d.%d crash-trail\n",
		ADDON_VERSION_MAJOR, ADDON_VERSION_MINOR,
		ADDON_VERSION_BUILD, ADDON_VERSION_REVISION);
	std::fprintf(f,
		"written %04u-%02u-%02u %02u:%02u:%02u.%03u  (tick=GetTickCount ms; +col = gap from previous note)\n",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	std::fprintf(f, "notes=%d seq=%u sticky=%s phase=%s detail=%d\n",
		gCount, gNoteSeq, gStickyMark[0] ? gStickyMark : "(none)",
		gPhase[0] ? gPhase : "idle", gDetailFrames);
	if (gHbStatus[0])
		std::fprintf(f, "hb %s (tick=%lu)\n", gHbStatus, static_cast<unsigned long>(gHbTick));
	WriteTrailToFile(f);
	std::fflush(f);
	std::fclose(f);
	gLastFlushMs = GetTickCount();
}

void NoteUnlocked(const char* tag)
{
	if (!tag || !tag[0])
		return;
	Entry& e = gRing[gHead];
	e.tick = GetTickCount();
	std::snprintf(e.tag, sizeof(e.tag), "%s", tag);
	gHead = (gHead + 1) % kRing;
	if (gCount < kRing)
		++gCount;
	++gNotesSinceFlush;
	++gNoteSeq;
	std::snprintf(gStickyMark, sizeof(gStickyMark), "%s", tag);
	gStickyMarkTick = e.tick;
	const bool critical = CriticalFlushTag(tag);
	const DWORD now = e.tick;
	const bool rateOk = (gLastFlushMs == 0u) || (now - gLastFlushMs) >= 150u;
	/* Critical always; else batch ≥24 notes and ≥150ms since last write. */
	if (critical || (gNotesSinceFlush >= 24 && rateOk))
	{
		gNotesSinceFlush = 0;
		WriteTrailUnlocked();
	}
}
}

void CrashTrail::Note(const char* tag)
{
	if (!tag || !tag[0])
		return;
	EnsureCs();
	EnterCriticalSection(&gCs);
	NoteUnlocked(tag);
	LeaveCriticalSection(&gCs);
}

void CrashTrail::NoteF(const char* fmt, ...)
{
	if (!fmt || !fmt[0])
		return;
	char buf[kTagMax];
	va_list ap;
	va_start(ap, fmt);
	std::vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	Note(buf);
}

void CrashTrail::Mark(const char* tag)
{
	if (!tag || !tag[0])
		return;
	EnsureCs();
	EnterCriticalSection(&gCs);
	char buf[kTagMax];
	std::snprintf(buf, sizeof(buf), "mark:%s", tag);
	NoteUnlocked(buf);
	LeaveCriticalSection(&gCs);
}

bool CrashTrail::DetailArmed()
{
	if (!WinePadOpen::Soft())
		return false;
	if (gDetailFrames > 0)
		return true;
	if (WinePadOpen::CompanionSettleFrames() > 0)
		return true;
	if (WatchPadDetail::gSoftStopPhase > 0)
		return true;
	if (WatchPadDetail::gDeferStopFrames > 0)
		return true;
	return false;
}

void CrashTrail::ArmDetail(int frames)
{
	if (frames < 1)
		frames = 1;
	EnsureCs();
	EnterCriticalSection(&gCs);
	if (frames > gDetailFrames)
		gDetailFrames = frames;
	LeaveCriticalSection(&gCs);
}

void CrashTrail::Tick()
{
	if (gDetailFrames > 0)
		--gDetailFrames;
}

void CrashTrail::HeartbeatIfHot()
{
	if (!WinePadOpen::Soft())
		return;
	const bool mirrorHot = G::ShowWatchMirror
		|| WatchPadDetail::gWantMirrorWhenReady
		|| WatchPadDetail::gSoftStopPhase > 0
		|| WatchCapture::IsCapturing();
	const bool pads = AnyCompanionPadOpen();
	if (!mirrorHot && !pads)
		return;

	static DWORD sLastHb = 0;
	const DWORD now = GetTickCount();
	/* Snapshot-only status — do not Note/flush on the render thread (orphan
	   17:42 tipped with sticky hb: while Mirror streamed; 2s fwrite was enough). */
	if (sLastHb != 0 && (now - sLastHb) < 5000u)
		return;
	sLastHb = now;
	gHbTick = now;
	std::snprintf(gHbStatus, sizeof(gHbStatus),
		"mirror=%d cap=%d stream=%d pads=%d settle=%d softstop=%d deferStop=%d busy=%d wiki=%d phase=%s ArcDPS=%d",
		G::ShowWatchMirror ? 1 : 0,
		WatchCapture::IsCapturing() ? 1 : 0,
		WatchCapture::IsStreaming() ? 1 : 0,
		pads ? 1 : 0,
		WinePadOpen::CompanionSettleFrames(),
		WatchPadDetail::gSoftStopPhase,
		WatchPadDetail::gDeferStopFrames,
		WinePadOpen::SoftWorkBusy() ? 1 : 0,
		G::ShowWiki ? 1 : 0,
		gPhase[0] ? gPhase : "idle",
		GetModuleHandleW(L"ArcDPS.dll") ? 1 : 0);
}

void CrashTrail::SetPhase(const char* phase)
{
	if (!phase || !phase[0])
		return;
	std::snprintf(gPhase, sizeof(gPhase), "%s", phase);
	gPhaseTick = GetTickCount();
	/* Keep sticky pointing at phase so orphan tips show last Nexus slot. */
	std::snprintf(gStickyMark, sizeof(gStickyMark), "phase:%s", gPhase);
	gStickyMarkTick = gPhaseTick;
}

const char* CrashTrail::Phase()
{
	return gPhase[0] ? gPhase : "idle";
}

CrashTrail::Scope::Scope(const char* enter, const char* leave)
{
	if (!enter || !enter[0] || !DetailArmed())
		return;
	Note(enter);
	on_ = true;
	if (leave && leave[0])
		std::snprintf(leave_, sizeof(leave_), "%s", leave);
}

CrashTrail::Scope::~Scope()
{
	if (on_ && leave_[0])
		Note(leave_);
}

void CrashTrail::Flush()
{
	EnsureCs();
	EnterCriticalSection(&gCs);
	gNotesSinceFlush = 0;
	WriteTrailUnlocked();
	LeaveCriticalSection(&gCs);
}

void CrashTrail::Install()
{
	EnsureCs();
	if (gInstalled)
		return;
	EnterCriticalSection(&gCs);
	MigrateLegacyCrashFiles();
	PromoteOrphanTrailUnlocked();
	LeaveCriticalSection(&gCs);

	HardenSelfTlsCallbacks();

	gVectored = AddVectoredExceptionHandler(1, VectoredHandler);
	gPrevFilter = SetUnhandledExceptionFilter(UnhandledFilter);
	gInstalled = true;
	Note("install");
	NoteF("coexist ArcDPS=%d d912pxy=%d wine=%d",
		GetModuleHandleW(L"ArcDPS.dll") ? 1 : 0,
		GetModuleHandleW(L"d912pxy.dll") ? 1 : 0,
		EiRuntime::IsWine() ? 1 : 0);
	Flush();
}

void CrashTrail::Shutdown()
{
	EnsureCs();
	EnterCriticalSection(&gCs);
	if (gInstalled)
	{
		if (gVectored)
		{
			RemoveVectoredExceptionHandler(gVectored);
			gVectored = nullptr;
		}
		SetUnhandledExceptionFilter(gPrevFilter);
		gPrevFilter = nullptr;
		gInstalled = false;
	}
	LeaveCriticalSection(&gCs);
	Note("shutdown");
	Flush();
}
