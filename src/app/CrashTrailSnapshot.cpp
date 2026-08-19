#include "CrashTrailInternal.h"

#include "AddonVersion.h"
#include "EiRuntime.h"
#include "Globals.h"
#include "WatchCapture.h"
#include "WatchPadInternal.h"
#include "WinePadOpen.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>
#include <tlhelp32.h>

using namespace CrashTrailDetail;

namespace CrashTrailDetail
{
const char* ExceptionName(DWORD code)
{
	switch (code)
	{
	case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
	case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
	case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
	case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
	case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
	case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
	default: return "OTHER";
	}
}

void DescribeModuleAt(FILE* f, const char* label, const void* addr)
{
	if (!f || !addr)
		return;
	HMODULE mod = nullptr;
	if (!GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
				GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(addr),
			&mod) ||
		!mod)
	{
		std::fprintf(f, "%s addr=%p module=(unknown)\n", label, addr);
		return;
	}
	wchar_t modPath[MAX_PATH]{};
	GetModuleFileNameW(mod, modPath, MAX_PATH);
	const auto base = reinterpret_cast<uintptr_t>(mod);
	const auto a = reinterpret_cast<uintptr_t>(addr);
	std::fprintf(f, "%s addr=%p module=%ls base=%p rva=0x%llX\n",
		label, addr, modPath, reinterpret_cast<void*>(base),
		static_cast<unsigned long long>(a >= base ? a - base : 0));
}

void WriteModule(FILE* f, const wchar_t* name)
{
	if (!f || !name)
		return;
	HMODULE mod = GetModuleHandleW(name);
	if (!mod)
	{
		std::fprintf(f, "  %ls=(not loaded)\n", name);
		return;
	}
	wchar_t path[MAX_PATH]{};
	GetModuleFileNameW(mod, path, MAX_PATH);
	std::fprintf(f, "  %ls base=%p path=%ls\n",
		name, reinterpret_cast<void*>(mod), path);
}

void WriteKnownModules(FILE* f)
{
	if (!f)
		return;
	std::fprintf(f, "modules\n");
	HMODULE self = nullptr;
	if (GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
				GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&CrashTrail::Note),
			&self) &&
		self)
	{
		wchar_t path[MAX_PATH]{};
		GetModuleFileNameW(self, path, MAX_PATH);
		std::fprintf(f, "  self base=%p path=%ls\n",
			reinterpret_cast<void*>(self), path);
	}
	WriteModule(f, L"GW2-InGame-Helper.dll");
	WriteModule(f, L"ArcDPS.dll");
	WriteModule(f, L"d912pxy.dll");
	WriteModule(f, L"d3d9.dll");
	WriteModule(f, L"d3d11.dll");
	WriteModule(f, L"dxgi.dll");
	WriteModule(f, L"d3d12.dll");
	WriteModule(f, L"ntdll.dll");
	WriteModule(f, L"kernel32.dll");
	WriteModule(f, L"user32.dll");
	WriteModule(f, L"Gw2-64.exe");

	/* Toolhelp32 walks the PEB LDR list — unsafe in a Wine vectored handler
	   (nested AVs / re-entrancy). Skip under Wine while snapshotting. */
	std::fprintf(f, "addon_modules\n");
	const bool skipToolhelp = EiRuntime::IsWine() && gInCrashSnapshot;
	if (skipToolhelp)
	{
		std::fprintf(f, "  (skipped toolhelp on wine crash handler)\n");
		std::fprintf(f, "addon_modules_count=-1\n");
		std::fprintf(f, "coexist ArcDPS=%d d912pxy=%d\n",
			GetModuleHandleW(L"ArcDPS.dll") ? 1 : 0,
			GetModuleHandleW(L"d912pxy.dll") ? 1 : 0);
		return;
	}
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
		GetCurrentProcessId());
	if (snap == INVALID_HANDLE_VALUE)
	{
		std::fprintf(f, "  (snapshot failed)\n");
		return;
	}
	MODULEENTRY32W me{};
	me.dwSize = sizeof(me);
	int n = 0;
	if (Module32FirstW(snap, &me))
	{
		do
		{
			const wchar_t* path = me.szExePath;
			bool isAddon = false;
			for (const wchar_t* p = path; *p; ++p)
			{
				/* case-insensitive "addons" */
				if ((p[0] == L'a' || p[0] == L'A')
					&& (p[1] == L'd' || p[1] == L'D')
					&& (p[2] == L'd' || p[2] == L'D')
					&& (p[3] == L'o' || p[3] == L'O')
					&& (p[4] == L'n' || p[4] == L'N')
					&& (p[5] == L's' || p[5] == L'S'))
				{
					isAddon = true;
					break;
				}
			}
			if (!isAddon)
				continue;
			std::fprintf(f, "  %ls base=%p\n", me.szModule,
				reinterpret_cast<void*>(me.modBaseAddr));
			++n;
		} while (Module32NextW(snap, &me));
	}
	CloseHandle(snap);
	std::fprintf(f, "addon_modules_count=%d\n", n);
	std::fprintf(f, "coexist ArcDPS=%d d912pxy=%d\n",
		GetModuleHandleW(L"ArcDPS.dll") ? 1 : 0,
		GetModuleHandleW(L"d912pxy.dll") ? 1 : 0);
}

void WriteMemory(FILE* f)
{
	if (!f)
		return;
	MEMORYSTATUSEX ms{};
	ms.dwLength = sizeof(ms);
	if (GlobalMemoryStatusEx(&ms))
	{
		std::fprintf(f,
			"memory load=%lu%% availPhys=%lluMB totalPhys=%lluMB availVirt=%lluMB\n",
			static_cast<unsigned long>(ms.dwMemoryLoad),
			static_cast<unsigned long long>(ms.ullAvailPhys / (1024ull * 1024ull)),
			static_cast<unsigned long long>(ms.ullTotalPhys / (1024ull * 1024ull)),
			static_cast<unsigned long long>(ms.ullAvailVirtual / (1024ull * 1024ull)));
	}
}

void WriteImGui(FILE* f)
{
	if (!f)
		return;
	ImGuiContext* ctx = ImGui::GetCurrentContext();
	if (!ctx)
	{
		std::fprintf(f, "imgui=(no context)\n");
		return;
	}
	const ImGuiIO& io = ctx->IO;
	std::fprintf(f,
		"imgui frame=%d dt=%.4f fps=%.1f display=%.0fx%.0f "
		"wantMouse=%d wantKey=%d windows=%d\n",
		ctx->FrameCount, io.DeltaTime, io.Framerate,
		io.DisplaySize.x, io.DisplaySize.y,
		io.WantCaptureMouse ? 1 : 0, io.WantCaptureKeyboard ? 1 : 0,
		ctx->Windows.Size);
	/* Active / hovered window names help pin Begin crashes. */
	if (ctx->CurrentWindow)
		std::fprintf(f, "imgui current=%s\n",
			ctx->CurrentWindow->Name ? ctx->CurrentWindow->Name : "(null)");
	if (ctx->HoveredWindow)
		std::fprintf(f, "imgui hovered=%s\n",
			ctx->HoveredWindow->Name ? ctx->HoveredWindow->Name : "(null)");
}

void WriteUiState(FILE* f)
{
	if (!f)
		return;
	std::fprintf(f, "runtime wine=%d soft=%d detail=%d seq=%u\n",
		EiRuntime::IsWine() ? 1 : 0, WinePadOpen::Soft() ? 1 : 0,
		gDetailFrames, gNoteSeq);
	std::fprintf(f, "sticky tick=%lu mark=%s\n",
		static_cast<unsigned long>(gStickyMarkTick),
		gStickyMark[0] ? gStickyMark : "(none)");
	std::fprintf(f, "phase tick=%lu name=%s\n",
		static_cast<unsigned long>(gPhaseTick),
		gPhase[0] ? gPhase : "idle");
	if (gHbStatus[0])
		std::fprintf(f, "hb tick=%lu %s\n",
			static_cast<unsigned long>(gHbTick), gHbStatus);
	std::fprintf(f, "coexist ArcDPS=%d d912pxy=%d\n",
		GetModuleHandleW(L"ArcDPS.dll") ? 1 : 0,
		GetModuleHandleW(L"d912pxy.dll") ? 1 : 0);
	std::fprintf(f,
		"ui ShowWatchMirror=%d ShowWatch=%d ShowEvents=%d ShowSettings=%d "
		"ShowInstances=%d ShowVault=%d ShowAchievements=%d "
		"ShowPathing=%d ShowNotes=%d ShowAccount=%d "
		"ShowEconomy=%d ShowCrafting=%d ShowLogManager=%d ShowCompass=%d ShowWiki=%d\n",
		G::ShowWatchMirror ? 1 : 0,
		G::ShowWatch ? 1 : 0,
		G::ShowEvents ? 1 : 0,
		G::ShowSettings ? 1 : 0,
		G::ShowInstances ? 1 : 0,
		G::ShowVault ? 1 : 0,
		G::ShowAchievements ? 1 : 0,
		G::ShowPathingGuides ? 1 : 0,
		G::ShowNotes ? 1 : 0,
		G::ShowAccount ? 1 : 0,
		G::ShowEconomy ? 1 : 0,
		G::ShowCrafting ? 1 : 0,
		G::ShowLogManager ? 1 : 0,
		G::ShowCompassPad ? 1 : 0,
		G::ShowWiki ? 1 : 0);
	std::fprintf(f,
		"watch capturing=%d streaming=%d hasContent=%d wantMirror=%d "
		"softStopPhase=%d softStopFrames=%d deferMirrorOpen=%d "
		"deferStop=%d reopenGate=%d\n",
		WatchCapture::IsCapturing() ? 1 : 0,
		WatchCapture::IsStreaming() ? 1 : 0,
		WatchCapture::HasContent() ? 1 : 0,
		WatchPadDetail::gWantMirrorWhenReady ? 1 : 0,
		WatchPadDetail::gSoftStopPhase,
		WatchPadDetail::gSoftStopFrames,
		WatchPadDetail::gDeferMirrorOpenFrames,
		WatchPadDetail::gDeferStopFrames,
		WatchPadDetail::gReopenGateFrames);
	std::fprintf(f,
		"softopen pending=%s frames=%d settle=%d quiet=%d firedThisFrame=%d "
		"softWorkBusy=%d waitingOnMirror=%d\n",
		WinePadOpen::PendingCompanionName(),
		WinePadOpen::CompanionOpenFrames(),
		WinePadOpen::CompanionSettleFrames(),
		WinePadOpen::WatchMirrorQuietFrames(),
		WinePadOpen::CompanionFiredThisFrame() ? 1 : 0,
		WinePadOpen::SoftWorkBusy() ? 1 : 0,
		WinePadOpen::CompanionWaitingOnMirror() ? 1 : 0);
	std::fprintf(f, "mirror_gpu=%s status=%s\n",
		WatchCapture::MirrorGpuPathText(),
		WatchCapture::StatusText());
}

void WriteLastTags(FILE* f, int want)
{
	if (!f || want <= 0)
		return;
	const int n = gCount < kRing ? gCount : kRing;
	const int take = want < n ? want : n;
	std::fprintf(f, "last_tags (%d)\n", take);
	for (int i = take; i > 0; --i)
	{
		const Entry& e = gRing[(gHead - i + kRing) % kRing];
		std::fprintf(f, "  %s\n", e.tag);
	}
}

void WriteExceptionDetail(FILE* f, EXCEPTION_POINTERS* ep, const char* how)
{
	if (!f)
		return;
	SYSTEMTIME st{};
	GetLocalTime(&st);
	std::fprintf(f, "GW2-InGame-Helper %d.%d.%d.%d crash snapshot\n",
		ADDON_VERSION_MAJOR, ADDON_VERSION_MINOR,
		ADDON_VERSION_BUILD, ADDON_VERSION_REVISION);
	std::fprintf(f, "when %04u-%02u-%02u %02u:%02u:%02u.%03u\n",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	std::fprintf(f, "how=%s pid=%lu tid=%lu tick=%lu\n",
		how ? how : "exception",
		static_cast<unsigned long>(GetCurrentProcessId()),
		static_cast<unsigned long>(GetCurrentThreadId()),
		static_cast<unsigned long>(GetTickCount()));

	WriteLastTags(f, 16);
	WriteUiState(f);
	WriteImGui(f);
	WriteMemory(f);
	WriteKnownModules(f);

	if (ep && ep->ExceptionRecord)
	{
		const EXCEPTION_RECORD* er = ep->ExceptionRecord;
		std::fprintf(f, "exception code=0x%08lX (%s) flags=0x%08lX params=%lu\n",
			static_cast<unsigned long>(er->ExceptionCode),
			ExceptionName(er->ExceptionCode),
			static_cast<unsigned long>(er->ExceptionFlags),
			static_cast<unsigned long>(er->NumberParameters));
		DescribeModuleAt(f, "fault", er->ExceptionAddress);
		if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
		{
			const ULONG_PTR op = er->ExceptionInformation[0];
			const void* faultVa = reinterpret_cast<const void*>(er->ExceptionInformation[1]);
			std::fprintf(f, "av op=%s va=%p\n",
				op == 0 ? "read" : op == 1 ? "write" : op == 8 ? "execute" : "other",
				faultVa);
		}
		else if (er->ExceptionCode == EXCEPTION_IN_PAGE_ERROR && er->NumberParameters >= 3)
		{
			std::fprintf(f, "in_page op=%llu va=%p ntstatus=0x%08llX\n",
				static_cast<unsigned long long>(er->ExceptionInformation[0]),
				reinterpret_cast<const void*>(er->ExceptionInformation[1]),
				static_cast<unsigned long long>(er->ExceptionInformation[2]));
		}
		if (er->ExceptionRecord)
			DescribeModuleAt(f, "nested", er->ExceptionRecord->ExceptionAddress);
	}
	else
	{
		std::fprintf(f, "exception=(none — hard tip / orphan trail)\n");
	}

#if defined(_M_X64) || defined(__x86_64__)
	if (ep && ep->ContextRecord)
	{
		const CONTEXT* c = ep->ContextRecord;
		std::fprintf(f,
			"context rip=%p rsp=%p rbp=%p rax=%p rbx=%p rcx=%p rdx=%p "
			"rsi=%p rdi=%p r8=%p r9=%p\n",
			reinterpret_cast<void*>(c->Rip),
			reinterpret_cast<void*>(c->Rsp),
			reinterpret_cast<void*>(c->Rbp),
			reinterpret_cast<void*>(c->Rax),
			reinterpret_cast<void*>(c->Rbx),
			reinterpret_cast<void*>(c->Rcx),
			reinterpret_cast<void*>(c->Rdx),
			reinterpret_cast<void*>(c->Rsi),
			reinterpret_cast<void*>(c->Rdi),
			reinterpret_cast<void*>(c->R8),
			reinterpret_cast<void*>(c->R9));
		DescribeModuleAt(f, "rip", reinterpret_cast<const void*>(c->Rip));
	}
#endif

	WriteStack(f, ep);
	std::fprintf(f, "trail notes=%d\n", gCount);
	WriteTrailToFile(f);
}

void WriteCrashSnapshotUnlocked(EXCEPTION_POINTERS* ep, const char* how)
{
	if (InterlockedCompareExchange(&gCrashSnapDone, 1, 0) != 0)
		return;

	InterlockedExchange(&gInCrashSnapshot, 1);

	SYSTEMTIME st{};
	GetLocalTime(&st);
	const std::wstring dir = MakeStampFolder(st);
	if (dir.empty())
	{
		InterlockedExchange(&gInCrashSnapshot, 0);
		return;
	}
	const std::wstring path = JoinUnder(dir, L"snapshot.txt");
	if (path.empty())
	{
		InterlockedExchange(&gInCrashSnapshot, 0);
		return;
	}
	FILE* f = _wfopen(path.c_str(), L"wb");
	if (!f)
	{
		InterlockedExchange(&gInCrashSnapshot, 0);
		return;
	}
	WriteExceptionDetail(f, ep, how);
	std::fflush(f);
	std::fclose(f);

	WriteTrailUnlocked();
	CopyFileBestEffort(TrailPath(), JoinUnder(dir, L"crash-trail.txt"));

	const wchar_t* leaf = dir.c_str();
	for (const wchar_t* p = dir.c_str(); *p; ++p)
	{
		if (*p == L'\\' || *p == L'/')
			leaf = p + 1;
	}
	AppendIndexLine(st, how, ep, leaf);
	PruneOldStampFolders();
	InterlockedExchange(&gInCrashSnapshot, 0);
}

bool InterestingException(DWORD code)
{
	switch (code)
	{
	case EXCEPTION_ACCESS_VIOLATION:
	case EXCEPTION_STACK_OVERFLOW:
	case EXCEPTION_ILLEGAL_INSTRUCTION:
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
	case EXCEPTION_PRIV_INSTRUCTION:
	case EXCEPTION_IN_PAGE_ERROR:
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		return true;
	default:
		return false;
	}
}

LONG WINAPI VectoredHandler(EXCEPTION_POINTERS* ep)
{
	if (!ep || !ep->ExceptionRecord)
		return EXCEPTION_CONTINUE_SEARCH;
	if (!InterestingException(ep->ExceptionRecord->ExceptionCode))
		return EXCEPTION_CONTINUE_SEARCH;
	EnsureCs();
	EnterCriticalSection(&gCs);
	WriteCrashSnapshotUnlocked(ep, "vectored");
	LeaveCriticalSection(&gCs);
	return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI UnhandledFilter(EXCEPTION_POINTERS* ep)
{
	EnsureCs();
	EnterCriticalSection(&gCs);
	WriteCrashSnapshotUnlocked(ep, "unhandled");
	LeaveCriticalSection(&gCs);
	if (gPrevFilter)
		return gPrevFilter(ep);
	return EXCEPTION_CONTINUE_SEARCH;
}
}
