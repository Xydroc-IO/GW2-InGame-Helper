#include "CrashTrail.h"

#include "AddonPaths.h"
#include "AddonVersion.h"
#include "EiRuntime.h"
#include "Globals.h"
#include "WatchCapture.h"
#include "WatchPadInternal.h"
#include "WinePadOpen.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>
#include <tlhelp32.h>

namespace
{
	constexpr int kRing = 128;
	constexpr int kTagMax = 192;
	constexpr int kCrashSlots = 3; /* crash-0 newest … crash-2 oldest */
	constexpr int kStackFrames = 32;

	struct Entry
	{
		DWORD tick = 0;
		char  tag[kTagMax]{};
	};

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
	char             gStickyMark[kTagMax]{};
	DWORD            gStickyMarkTick = 0;
	char             gPhase[64] = "idle";
	DWORD            gPhaseTick = 0;
	int              gDetailFrames = 0;
	unsigned         gNoteSeq = 0;

	void EnsureCs()
	{
		if (gCsReady)
			return;
		InitializeCriticalSection(&gCs);
		gCsReady = true;
	}

	std::wstring DataFile(const wchar_t* name)
	{
		std::wstring p = AddonPaths::DataDir();
		if (p.empty())
			return {};
		if (p.back() != L'\\' && p.back() != L'/')
			p.push_back(L'\\');
		p += name;
		return p;
	}

	std::wstring TrailPath() { return DataFile(L"crash-trail.txt"); }
	std::wstring CrashLogPath() { return DataFile(L"crash.log"); }

	std::wstring CrashSlotPath(int slot)
	{
		wchar_t name[32];
		std::swprintf(name, 32, L"crash-%d.txt", slot);
		return DataFile(name);
	}

	bool ShouldFlushTag(const char* tag)
	{
		if (!tag)
			return false;
		return std::strstr(tag, "softopen")
			|| std::strstr(tag, "softfire")
			|| std::strstr(tag, "softstop")
			|| std::strstr(tag, "pads:")
			|| std::strstr(tag, "ev:")
			|| std::strstr(tag, "cap:")
			|| std::strstr(tag, "watch:")
			|| std::strstr(tag, "ui:")
			|| std::strstr(tag, "world:")
			|| std::strstr(tag, "cef:")
			|| std::strstr(tag, "gpu:")
			|| std::strstr(tag, "save:")
			|| std::strstr(tag, "hb:")
			|| std::strstr(tag, "upload")
			|| std::strstr(tag, "mark:");
	}

	bool AnyCompanionPadOpen()
	{
		return G::ShowNotes || G::ShowAccount || G::ShowTpWatch || G::ShowLookup
			|| G::ShowWallet || G::ShowVault || G::ShowEvents || G::ShowLogManager
			|| G::ShowEconomy || G::ShowInstances || G::ShowCompletion || G::ShowFarming
			|| G::ShowPathingGuides || G::ShowTrailTools || G::ShowCompassPad
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
		WriteTrailToFile(f);
		std::fflush(f);
		std::fclose(f);
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
		const bool flushNow = gNotesSinceFlush >= 3 || ShouldFlushTag(tag);
		if (flushNow)
		{
			gNotesSinceFlush = 0;
			WriteTrailUnlocked();
		}
	}

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

		/* All modules with "addons" in path — coexistence suspects. */
		std::fprintf(f, "addon_modules\n");
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
		std::fprintf(f, "coexist ArcDPS=%d d912pxy=%d\n",
			GetModuleHandleW(L"ArcDPS.dll") ? 1 : 0,
			GetModuleHandleW(L"d912pxy.dll") ? 1 : 0);
		std::fprintf(f,
			"ui ShowWatchMirror=%d ShowWatch=%d ShowEvents=%d ShowSettings=%d "
			"ShowInstances=%d ShowVault=%d ShowFarming=%d ShowCompletion=%d "
			"ShowTrailTools=%d ShowPathing=%d ShowNotes=%d ShowAccount=%d "
			"ShowEconomy=%d ShowLogManager=%d ShowCompass=%d ShowWiki=%d\n",
			G::ShowWatchMirror ? 1 : 0,
			G::ShowWatch ? 1 : 0,
			G::ShowEvents ? 1 : 0,
			G::ShowSettings ? 1 : 0,
			G::ShowInstances ? 1 : 0,
			G::ShowVault ? 1 : 0,
			G::ShowFarming ? 1 : 0,
			G::ShowCompletion ? 1 : 0,
			G::ShowTrailTools ? 1 : 0,
			G::ShowPathingGuides ? 1 : 0,
			G::ShowNotes ? 1 : 0,
			G::ShowAccount ? 1 : 0,
			G::ShowEconomy ? 1 : 0,
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

	void WriteStack(FILE* f)
	{
		if (!f)
			return;
		void* frames[kStackFrames]{};
		const USHORT n = CaptureStackBackTrace(0, kStackFrames, frames, nullptr);
		std::fprintf(f, "stack frames=%u\n", static_cast<unsigned>(n));
		for (USHORT i = 0; i < n; ++i)
		{
			char label[32];
			std::snprintf(label, sizeof(label), "  #%u", static_cast<unsigned>(i));
			DescribeModuleAt(f, label, frames[i]);
		}
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

		WriteStack(f);
		std::fprintf(f, "trail notes=%d\n", gCount);
		WriteTrailToFile(f);
	}

	void RotateCrashSlots()
	{
		const std::wstring oldest = CrashSlotPath(kCrashSlots - 1);
		if (!oldest.empty())
			DeleteFileW(oldest.c_str());
		for (int i = kCrashSlots - 2; i >= 0; --i)
		{
			const std::wstring from = CrashSlotPath(i);
			const std::wstring to = CrashSlotPath(i + 1);
			if (from.empty() || to.empty())
				continue;
			MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING);
		}
	}

	void AppendIndexLine(const char* how, EXCEPTION_POINTERS* ep)
	{
		const std::wstring path = CrashLogPath();
		if (path.empty())
			return;
		FILE* f = _wfopen(path.c_str(), L"ab");
		if (!f)
			return;
		SYSTEMTIME st{};
		GetLocalTime(&st);
		const char* code = "none";
		char codeBuf[32];
		if (ep && ep->ExceptionRecord)
		{
			std::snprintf(codeBuf, sizeof(codeBuf), "0x%08lX",
				static_cast<unsigned long>(ep->ExceptionRecord->ExceptionCode));
			code = codeBuf;
		}
		std::fprintf(f,
			"%04u-%02u-%02u %02u:%02u:%02u.%03u  %s  code=%s  sticky=%s  -> crash-0.txt\n",
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
			how ? how : "?", code, gStickyMark[0] ? gStickyMark : "(empty)");
		std::fflush(f);
		std::fclose(f);
	}

	void WriteCrashSnapshotUnlocked(EXCEPTION_POINTERS* ep, const char* how)
	{
		if (InterlockedCompareExchange(&gCrashSnapDone, 1, 0) != 0)
			return;

		RotateCrashSlots();
		const std::wstring path = CrashSlotPath(0);
		if (path.empty())
			return;
		FILE* f = _wfopen(path.c_str(), L"wb");
		if (!f)
			return;
		WriteExceptionDetail(f, ep, how);
		std::fflush(f);
		std::fclose(f);
		AppendIndexLine(how, ep);
		WriteTrailUnlocked();
	}

	bool ReadDiskTrailLastTag(char* out, size_t outN, int* outNotes)
	{
		if (out && outN)
			out[0] = 0;
		if (outNotes)
			*outNotes = 0;
		const std::wstring path = TrailPath();
		if (path.empty())
			return false;
		FILE* f = _wfopen(path.c_str(), L"rb");
		if (!f)
			return false;
		char line[512];
		char lastTag[kTagMax]{};
		int notes = 0;
		bool sawShutdown = false;
		while (std::fgets(line, sizeof(line), f))
		{
			if (std::strncmp(line, "notes=", 6) == 0)
			{
				notes = std::atoi(line + 6);
				continue;
			}
			const char* tag = nullptr;
			if (line[0] == '+')
			{
				const char* t = std::strstr(line, "  ");
				if (t)
				{
					t = std::strstr(t + 2, "  ");
					if (t)
						tag = t + 2;
				}
			}
			else if (line[0] >= '0' && line[0] <= '9')
			{
				const char* sp = std::strchr(line, ' ');
				if (sp)
					tag = sp + 1;
			}
			if (!tag || !tag[0])
				continue;
			std::snprintf(lastTag, sizeof(lastTag), "%s", tag);
			for (char* p = lastTag; *p; ++p)
			{
				if (*p == '\r' || *p == '\n')
				{
					*p = 0;
					break;
				}
			}
			if (std::strcmp(lastTag, "shutdown") == 0)
				sawShutdown = true;
		}
		std::fclose(f);
		if (out && outN)
			std::snprintf(out, outN, "%s", lastTag);
		if (outNotes)
			*outNotes = notes;
		if (sawShutdown || lastTag[0] == 0)
			return false;
		if (std::strcmp(lastTag, "install") == 0)
			return false;
		if (std::strncmp(lastTag, "dedicated", 9) == 0)
			return false;
		return true;
	}

	void PromoteOrphanTrailUnlocked()
	{
		char lastTag[kTagMax]{};
		int notes = 0;
		if (!ReadDiskTrailLastTag(lastTag, sizeof(lastTag), &notes))
			return;

		RotateCrashSlots();
		const std::wstring path = CrashSlotPath(0);
		if (path.empty())
			return;

		const std::wstring trail = TrailPath();
		FILE* src = _wfopen(trail.c_str(), L"rb");
		FILE* dst = _wfopen(path.c_str(), L"wb");
		if (!dst)
		{
			if (src)
				std::fclose(src);
			return;
		}
		SYSTEMTIME st{};
		GetLocalTime(&st);
		std::fprintf(dst, "GW2-InGame-Helper %d.%d.%d.%d crash snapshot\n",
			ADDON_VERSION_MAJOR, ADDON_VERSION_MINOR,
			ADDON_VERSION_BUILD, ADDON_VERSION_REVISION);
		std::fprintf(dst, "when %04u-%02u-%02u %02u:%02u:%02u.%03u\n",
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
		std::fprintf(dst,
			"how=orphan-trail (previous session tipped without SEH; promoted on reload)\n");
		std::fprintf(dst, "prior_last_tag=%s prior_notes=%d\n", lastTag, notes);
		WriteUiState(dst);
		WriteImGui(dst);
		WriteMemory(dst);
		WriteKnownModules(dst);
		std::fprintf(dst, "--- prior crash-trail.txt ---\n");
		if (src)
		{
			char buf[1024];
			size_t n;
			while ((n = std::fread(buf, 1, sizeof(buf), src)) > 0)
				std::fwrite(buf, 1, n, dst);
			std::fclose(src);
		}
		std::fflush(dst);
		std::fclose(dst);

		FILE* idx = _wfopen(CrashLogPath().c_str(), L"ab");
		if (idx)
		{
			std::fprintf(idx,
				"%04u-%02u-%02u %02u:%02u:%02u.%03u  orphan-trail  last=%s  -> crash-0.txt\n",
				st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
				lastTag);
			std::fflush(idx);
			std::fclose(idx);
		}
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
	if (sLastHb != 0 && (now - sLastHb) < 2000u)
		return;
	sLastHb = now;

	NoteF("hb:mirror=%d cap=%d stream=%d pads=%d settle=%d softstop=%d deferStop=%d busy=%d wiki=%d phase=%s ArcDPS=%d",
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
	/* Do NOT ArmDetail here — that kept DetailArmed forever and flooded the
	   ring with every-frame ui:/cef:/watch: spam, wiping softopen/softstop pins. */
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
	PromoteOrphanTrailUnlocked();
	LeaveCriticalSection(&gCs);

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
