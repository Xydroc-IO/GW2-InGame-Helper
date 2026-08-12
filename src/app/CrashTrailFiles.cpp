#include "CrashTrailInternal.h"

#include "AddonPaths.h"
#include "AddonVersion.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>

#include <windows.h>

using namespace CrashTrailDetail;

namespace CrashTrailDetail
{
std::wstring JoinUnder(const std::wstring& dir, const wchar_t* name)
{
	if (dir.empty() || !name || !name[0])
		return {};
	std::wstring p = dir;
	if (p.back() != L'\\' && p.back() != L'/')
		p.push_back(L'\\');
	p += name;
	return p;
}

std::wstring DataRootFile(const wchar_t* name)
{
	return JoinUnder(AddonPaths::DataDir(), name);
}

std::wstring TrailPath()
{
	return JoinUnder(AddonPaths::CrashLogsDir(), L"crash-trail.txt");
}

std::wstring CrashLogPath()
{
	return JoinUnder(AddonPaths::CrashLogsDir(), L"crash.log");
}

std::wstring LegacyTrailPath() { return DataRootFile(L"crash-trail.txt"); }
std::wstring LegacyCrashLogPath() { return DataRootFile(L"crash.log"); }

/* ASCII snprintf → wide: avoids MinGW/MSVC swprintf size-arg mismatches that
   produced a folder literally named "2" instead of YYYY-MM-DD_HH-MM-SS_mmm. */
bool FormatStampFolder(SYSTEMTIME st, char* out, size_t outN)
{
	if (!out || outN < 24)
		return false;
	const int n = std::snprintf(out, outN, "%04u-%02u-%02u_%02u-%02u-%02u_%03u",
		static_cast<unsigned>(st.wYear), static_cast<unsigned>(st.wMonth),
		static_cast<unsigned>(st.wDay), static_cast<unsigned>(st.wHour),
		static_cast<unsigned>(st.wMinute), static_cast<unsigned>(st.wSecond),
		static_cast<unsigned>(st.wMilliseconds));
	return n > 0 && static_cast<size_t>(n) < outN && out[0] >= '0' && out[0] <= '9';
}

std::wstring WidenPathName(const char* utf8)
{
	if (!utf8 || !utf8[0])
		return {};
	wchar_t w[128]{};
	if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, 128) <= 0)
		return {};
	return w;
}

/* Crash-Logs/YYYY-MM-DD_HH-MM-SS_mmm[/_N]/ — unique if tips share a ms. */
std::wstring MakeStampFolder(SYSTEMTIME st)
{
	const std::wstring root = AddonPaths::CrashLogsDir();
	if (root.empty())
		return {};
	char stamp[40]{};
	if (!FormatStampFolder(st, stamp, sizeof(stamp)))
		return {};
	for (int n = 0; n < 100; ++n)
	{
		char name[48]{};
		if (n == 0)
			std::snprintf(name, sizeof(name), "%s", stamp);
		else
			std::snprintf(name, sizeof(name), "%s_%d", stamp, n);
		const std::wstring wname = WidenPathName(name);
		if (wname.empty())
			continue;
		const std::wstring dir = JoinUnder(root, wname.c_str());
		if (dir.empty())
			continue;
		if (CreateDirectoryW(dir.c_str(), nullptr))
			return dir;
	}
	return {};
}

void CopyFileBestEffort(const std::wstring& from, const std::wstring& to)
{
	if (from.empty() || to.empty())
		return;
	CopyFileW(from.c_str(), to.c_str(), FALSE);
}

void PruneOldStampFolders()
{
	const std::wstring root = AddonPaths::CrashLogsDir();
	if (root.empty())
		return;
	std::wstring pattern = root;
	if (pattern.back() != L'\\' && pattern.back() != L'/')
		pattern.push_back(L'\\');
	pattern += L"*";

	wchar_t names[64][64]{};
	int count = 0;
	WIN32_FIND_DATAW fd{};
	HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE)
		return;
	do
	{
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			continue;
		if (fd.cFileName[0] == L'.')
			continue;
		/* Stamp folders are YYYY-… ; skip anything else. */
		if (fd.cFileName[0] < L'0' || fd.cFileName[0] > L'9')
			continue;
		if (count < 64)
		{
			std::swprintf(names[count], 64, L"%s", fd.cFileName);
			++count;
		}
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	if (count <= kMaxStampFolders)
		return;

	/* Lexicographic sort — zero-padded stamps sort by time. */
	for (int i = 0; i < count; ++i)
	{
		for (int j = i + 1; j < count; ++j)
		{
			if (std::wcscmp(names[j], names[i]) < 0)
			{
				wchar_t tmp[64];
				std::swprintf(tmp, 64, L"%s", names[i]);
				std::swprintf(names[i], 64, L"%s", names[j]);
				std::swprintf(names[j], 64, L"%s", tmp);
			}
		}
	}
	const int drop = count - kMaxStampFolders;
	for (int i = 0; i < drop; ++i)
	{
		std::wstring dir = JoinUnder(root, names[i]);
		if (dir.empty())
			continue;
		std::wstring snap = JoinUnder(dir, L"snapshot.txt");
		std::wstring trail = JoinUnder(dir, L"crash-trail.txt");
		if (!snap.empty())
			DeleteFileW(snap.c_str());
		if (!trail.empty())
			DeleteFileW(trail.c_str());
		RemoveDirectoryW(dir.c_str());
	}
}

void MigrateLegacyCrashFiles()
{
	const std::wstring root = AddonPaths::CrashLogsDir();
	if (root.empty())
		return;

	auto moveIfPresent = [&](const std::wstring& from, const std::wstring& to) {
		if (from.empty() || to.empty())
			return;
		if (GetFileAttributesW(from.c_str()) == INVALID_FILE_ATTRIBUTES)
			return;
		if (GetFileAttributesW(to.c_str()) != INVALID_FILE_ATTRIBUTES)
			return;
		MoveFileW(from.c_str(), to.c_str());
	};
	moveIfPresent(LegacyTrailPath(), TrailPath());
	moveIfPresent(LegacyCrashLogPath(), CrashLogPath());

	/* Old flat crash-0..2 → Crash-Logs/migrated-crash-N/snapshot.txt */
	for (int i = 0; i < 3; ++i)
	{
		wchar_t legacyName[32];
		std::swprintf(legacyName, 32, L"crash-%d.txt", i);
		const std::wstring from = DataRootFile(legacyName);
		if (from.empty() || GetFileAttributesW(from.c_str()) == INVALID_FILE_ATTRIBUTES)
			continue;
		wchar_t folder[48];
		std::swprintf(folder, 48, L"migrated-crash-%d", i);
		const std::wstring dir = AddonPaths::EnsureUnder(root, folder);
		const std::wstring to = JoinUnder(dir, L"snapshot.txt");
		if (!to.empty())
			MoveFileW(from.c_str(), to.c_str());
	}
}

void AppendIndexLine(const SYSTEMTIME& st, const char* how, EXCEPTION_POINTERS* ep,
	const wchar_t* stampFolder)
{
	const std::wstring path = CrashLogPath();
	if (path.empty())
		return;
	FILE* f = _wfopen(path.c_str(), L"ab");
	if (!f)
		return;
	const char* code = "none";
	char codeBuf[32];
	if (ep && ep->ExceptionRecord)
	{
		std::snprintf(codeBuf, sizeof(codeBuf), "0x%08lX",
			static_cast<unsigned long>(ep->ExceptionRecord->ExceptionCode));
		code = codeBuf;
	}
	std::fprintf(f,
		"%04u-%02u-%02u %02u:%02u:%02u.%03u  %s  code=%s  sticky=%s  -> %ls/snapshot.txt\n",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		how ? how : "?", code, gStickyMark[0] ? gStickyMark : "(empty)",
		stampFolder ? stampFolder : L"?");
	std::fflush(f);
	std::fclose(f);
}

bool ReadDiskTrailLastTag(char* out, size_t outN, int* outNotes)
{
	if (out && outN)
		out[0] = 0;
	if (outNotes)
		*outNotes = 0;
	std::wstring path = TrailPath();
	if (path.empty() || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
		path = LegacyTrailPath();
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

	SYSTEMTIME st{};
	GetLocalTime(&st);
	const std::wstring dir = MakeStampFolder(st);
	if (dir.empty())
		return;
	const std::wstring path = JoinUnder(dir, L"snapshot.txt");
	if (path.empty())
		return;

	std::wstring trail = TrailPath();
	if (trail.empty() || GetFileAttributesW(trail.c_str()) == INVALID_FILE_ATTRIBUTES)
		trail = LegacyTrailPath();
	FILE* src = _wfopen(trail.c_str(), L"rb");
	FILE* dst = _wfopen(path.c_str(), L"wb");
	if (!dst)
	{
		if (src)
			std::fclose(src);
		return;
	}
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

	CopyFileBestEffort(trail, JoinUnder(dir, L"crash-trail.txt"));

	const wchar_t* leaf = dir.c_str();
	for (const wchar_t* p = dir.c_str(); *p; ++p)
	{
		if (*p == L'\\' || *p == L'/')
			leaf = p + 1;
	}
	FILE* idx = _wfopen(CrashLogPath().c_str(), L"ab");
	if (idx)
	{
		std::fprintf(idx,
			"%04u-%02u-%02u %02u:%02u:%02u.%03u  orphan-trail  last=%s  -> %ls/snapshot.txt\n",
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
			lastTag, leaf);
		std::fflush(idx);
		std::fclose(idx);
	}
	PruneOldStampFolders();
}
}
