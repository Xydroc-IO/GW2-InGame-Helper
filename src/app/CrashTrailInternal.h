#pragma once

/* Shared state/helpers for CrashTrail*.cpp (not public API). */

#include "CrashTrail.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

namespace CrashTrailDetail
{
	constexpr int kRing = 128;
	constexpr int kTagMax = 192;
	constexpr int kMaxStampFolders = 30; /* retain newest timestamped tip folders */
	constexpr int kStackFrames = 32;

	struct Entry
	{
		DWORD tick = 0;
		char  tag[kTagMax]{};
	};

	extern CRITICAL_SECTION gCs;
	extern bool             gCsReady;
	extern bool             gInstalled;
	extern Entry            gRing[kRing];
	extern int              gHead;
	extern int              gCount;
	extern int              gNotesSinceFlush;
	extern PVOID            gVectored;
	extern LPTOP_LEVEL_EXCEPTION_FILTER gPrevFilter;
	extern volatile LONG    gCrashSnapDone;
	extern char             gStickyMark[kTagMax];
	extern DWORD            gStickyMarkTick;
	extern char             gPhase[64];
	extern DWORD            gPhaseTick;
	extern int              gDetailFrames;
	extern unsigned         gNoteSeq;
	extern DWORD            gLastFlushMs;
	extern char             gHbStatus[192];
	extern DWORD            gHbTick;
	extern volatile LONG    gInCrashSnapshot;

	void EnsureCs();
	void HardenSelfTlsCallbacks();

	/* CrashTrail.cpp — ring / trail */
	bool CriticalFlushTag(const char* tag);
	bool AnyCompanionPadOpen();
	void WriteTrailToFile(FILE* f);
	void WriteTrailUnlocked();
	void NoteUnlocked(const char* tag);

	/* CrashTrailFiles.cpp — paths / stamp folders */
	std::wstring JoinUnder(const std::wstring& dir, const wchar_t* name);
	std::wstring TrailPath();
	std::wstring CrashLogPath();
	std::wstring LegacyTrailPath();
	std::wstring LegacyCrashLogPath();
	std::wstring MakeStampFolder(SYSTEMTIME st);
	void CopyFileBestEffort(const std::wstring& from, const std::wstring& to);
	void PruneOldStampFolders();
	void MigrateLegacyCrashFiles();
	void AppendIndexLine(const SYSTEMTIME& st, const char* how, EXCEPTION_POINTERS* ep,
		const wchar_t* stampFolder);
	bool ReadDiskTrailLastTag(char* out, size_t outN, int* outNotes);
	void PromoteOrphanTrailUnlocked();

	/* CrashTrailSnapshot.cpp — dumps / SEH */
	const char* ExceptionName(DWORD code);
	void DescribeModuleAt(FILE* f, const char* label, const void* addr);
	void WriteModule(FILE* f, const wchar_t* name);
	void WriteKnownModules(FILE* f);
	void WriteMemory(FILE* f);
	void WriteImGui(FILE* f);
	void WriteUiState(FILE* f);
	void WriteStack(FILE* f, EXCEPTION_POINTERS* ep);
	void WriteLastTags(FILE* f, int want);
	void WriteExceptionDetail(FILE* f, EXCEPTION_POINTERS* ep, const char* how);
	void WriteCrashSnapshotUnlocked(EXCEPTION_POINTERS* ep, const char* how);
	bool InterestingException(DWORD code);
	LONG WINAPI VectoredHandler(EXCEPTION_POINTERS* ep);
	LONG WINAPI UnhandledFilter(EXCEPTION_POINTERS* ep);
}
