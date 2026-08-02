#include "LogManagerEi.h"

#include "LogManagerShared.h"

#include "AddonPaths.h"
#include "EiRuntime.h"
#include "Globals.h"
#include "Settings.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace LogManagerDetail
{
void EiStatusCb(const char* msg)
{
	if (!msg)
		return;
	std::snprintf(gEiStatus, sizeof(gEiStatus), "%s", msg);
	std::snprintf(gStatus, sizeof(gStatus), "%s", msg);
}

DWORD WINAPI EiInstallWorker(LPVOID)
{
	const std::wstring dir = AddonPaths::DataDir();
	const bool ok = EiRuntime::EnsureInstalled(dir.c_str(), EiStatusCb);
	if (ok)
	{
		ApplyManagedCliPath();
		char stamp[64]{};
		EiRuntime::InvalidateDotNet8Cache();
		if (EiRuntime::GetInstalledStamp(dir.c_str(), stamp, sizeof(stamp)))
		{
			if (EiRuntime::HasDotNet8Runtime())
				std::snprintf(gStatus, sizeof(gStatus), "Elite Insights %s ready.", stamp);
			else
				std::snprintf(gStatus, sizeof(gStatus),
					"Elite Insights %s installed — install .NET 8 Runtime to parse.", stamp);
		}
		else if (EiRuntime::HasDotNet8Runtime())
			std::snprintf(gStatus, sizeof(gStatus), "Elite Insights ready.");
		else
			std::snprintf(gStatus, sizeof(gStatus),
				"Elite Insights installed — install .NET 8 Runtime to parse.");
	}
	else if (!gEiStatus[0])
		std::snprintf(gStatus, sizeof(gStatus), "Elite Insights install failed.");
	gEiInstallBusy.store(false);
	return 0;
}

void BeginEiEnsure(bool force)
{
	if (gEiInstallBusy.exchange(true))
		return;

	/* Custom path already works — skip auto-update unless forced. */
	if (!force && PathExistsUtf8(G::EliteInsightsPath) && !IsManagedEiPath(G::EliteInsightsPath))
	{
		std::snprintf(gEiStatus, sizeof(gEiStatus), "Using custom Elite Insights path.");
		gEiInstallBusy.store(false);
		return;
	}

	std::snprintf(gEiStatus, sizeof(gEiStatus), "Checking Elite Insights updates…");
	std::snprintf(gStatus, sizeof(gStatus), "Checking Elite Insights updates…");
	if (gEiInstallThread)
	{
		CloseHandle(gEiInstallThread);
		gEiInstallThread = nullptr;
	}
	gEiInstallThread = CreateThread(nullptr, 0, EiInstallWorker, nullptr, 0, nullptr);
	if (!gEiInstallThread)
		gEiInstallBusy.store(false);
}

std::wstring EiConfPathW()
{
	return AddonPaths::DataDir() + L"\\ei-helper.conf";
}

/* ---------- Elite Insights ---------- */

std::wstring EiOutDirW()
{
	return AddonPaths::DataDir() + L"\\ei-out";
}

bool WriteEiConf()
{
	const std::wstring outDir = EiOutDirW();
	CreateDirectoryW(outDir.c_str(), nullptr);
	const std::string outUtf8 = WideToUtf8(outDir);
	const std::wstring conf = EiConfPathW();
	std::string body =
		"SaveOutHTML=false\n"
		"SaveOutCSV=false\n"
		"SaveOutJSON=true\n"
		"IndentJSON=false\n"
		"ParseMultipleLogs=false\n"
		"SingleThreaded=true\n"
		"AutoAdd=false\n"
		"AutoParse=false\n"
		"AutoUpload=false\n";
	if (!outUtf8.empty())
	{
		body += "OutLocation=";
		body += outUtf8;
		body += "\n";
	}
	return WriteFileUtf8(conf, body);
}

bool RunProcessCapture(const std::wstring& exe, const std::wstring& args, const std::wstring& cwd,
	std::string& stdoutUtf8, DWORD timeoutMs)
{
	stdoutUtf8.clear();
	SECURITY_ATTRIBUTES sa{};
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;
	HANDLE rd = nullptr, wr = nullptr;
	if (!CreatePipe(&rd, &wr, &sa, 0))
		return false;
	SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW si{};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	si.hStdOutput = wr;
	si.hStdError = wr;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

	PROCESS_INFORMATION pi{};
	std::wstring cmd = L"\"" + exe + L"\" " + args;
	std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
	cmdBuf.push_back(0);

	const BOOL ok = CreateProcessW(
		nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
		CREATE_NO_WINDOW, nullptr,
		cwd.empty() ? nullptr : cwd.c_str(),
		&si, &pi);
	CloseHandle(wr);
	if (!ok)
	{
		CloseHandle(rd);
		return false;
	}

	std::string buf;
	char chunk[4096];
	DWORD got = 0;
	const DWORD start = GetTickCount();
	for (;;)
	{
		DWORD avail = 0;
		if (PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) && avail > 0)
		{
			const DWORD toRead = avail > sizeof(chunk) ? sizeof(chunk) : avail;
			if (ReadFile(rd, chunk, toRead, &got, nullptr) && got > 0)
				buf.append(chunk, got);
		}
		const DWORD wait = WaitForSingleObject(pi.hProcess, 50);
		if (wait == WAIT_OBJECT_0)
		{
			while (ReadFile(rd, chunk, sizeof(chunk), &got, nullptr) && got > 0)
				buf.append(chunk, got);
			break;
		}
		if (GetTickCount() - start > timeoutMs)
		{
			TerminateProcess(pi.hProcess, 1);
			break;
		}
		if (gCancel.load())
		{
			TerminateProcess(pi.hProcess, 1);
			break;
		}
	}
	CloseHandle(rd);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	stdoutUtf8 = std::move(buf);
	return true;
}

std::wstring GuessJsonBeside(const std::wstring& logPath)
{
	/* EI typically writes <name>_*.json next to the log or in OutLocation. */
	std::wstring dir = logPath;
	const auto slash = dir.find_last_of(L"\\/");
	std::wstring folder = slash == std::wstring::npos ? L"." : dir.substr(0, slash);
	std::wstring stem = slash == std::wstring::npos ? dir : dir.substr(slash + 1);
	/* strip extensions */
	auto stripExt = [](std::wstring& s) {
		if (EndsWithI(s, L".evtc.zip"))
			s.resize(s.size() - 9);
		else if (EndsWithI(s, L".zevtc"))
			s.resize(s.size() - 6);
		else if (EndsWithI(s, L".evtc"))
			s.resize(s.size() - 5);
	};
	stripExt(stem);

	WIN32_FIND_DATAW fd{};
	const std::wstring pat = folder + L"\\" + stem + L"*.json";
	HANDLE h = FindFirstFileW(pat.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE)
		return {};
	std::wstring best;
	FILETIME bestTime{};
	do
	{
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		const std::wstring full = folder + L'\\' + fd.cFileName;
		if (best.empty() || CompareFileTime(&fd.ftLastWriteTime, &bestTime) > 0)
		{
			best = full;
			bestTime = fd.ftLastWriteTime;
		}
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	return best;
}

bool ParseOneWithEi(LogEntry& e)
{
	if (!G::EliteInsightsPath[0])
	{
		e.state = ParseState::Failed;
		e.parseError = "Set Elite Insights CLI path in settings.";
		return false;
	}
	const std::wstring exe = Utf8ToWide(G::EliteInsightsPath);
	if (!FileExistsW(exe))
	{
		e.state = ParseState::Failed;
		e.parseError = "Elite Insights CLI not found.";
		return false;
	}
	WriteEiConf();
	const std::wstring conf = EiConfPathW();
	std::wstring args = L"-c \"" + conf + L"\" \"" + e.pathW + L"\"";
	std::string output;
	if (!RunProcessCapture(exe, args, {}, output, kParseTimeoutMs))
	{
		e.state = ParseState::Failed;
		e.parseError = "Failed to launch Elite Insights.";
		return false;
	}

	/* Prefer generatedFiles from EI status line. */
	std::wstring jsonW;
	const char* gen = std::strstr(output.c_str(), "generatedFiles");
	if (gen)
	{
		const char* q = std::strchr(gen, '"');
		/* find first .json path in the status blob */
		const char* j = std::strstr(gen, ".json");
		if (j)
		{
			const char* start = j;
			while (start > gen && *start != '"' && *start != '\'')
				--start;
			if (*start == '"' || *start == '\'')
				++start;
			std::string path(start, static_cast<size_t>(j - start + 5));
			jsonW = Utf8ToWide(path.c_str());
		}
		(void)q;
	}
	if (jsonW.empty() || !FileExistsW(jsonW))
		jsonW = GuessJsonBeside(e.pathW);
	if (jsonW.empty() || !FileExistsW(jsonW))
		jsonW = GuessJsonBeside(EiOutDirW() + L"\\" + Utf8ToWide(e.fileName.c_str()));
	/* Also scan ei-out for newest json matching stem. */
	if (jsonW.empty() || !FileExistsW(jsonW))
	{
		std::wstring stem = Utf8ToWide(e.fileName.c_str());
		if (EndsWithI(stem, L".evtc.zip"))
			stem.resize(stem.size() - 9);
		else if (EndsWithI(stem, L".zevtc"))
			stem.resize(stem.size() - 6);
		else if (EndsWithI(stem, L".evtc"))
			stem.resize(stem.size() - 5);
		jsonW = GuessJsonBeside(EiOutDirW() + L"\\" + stem + L".zevtc");
	}

	if (jsonW.empty() || !FileExistsW(jsonW))
	{
		e.state = ParseState::Failed;
		e.parseError = "EI finished but no JSON found.";
		if (output.size() > 180)
			output.resize(180);
		if (!output.empty())
			e.parseError += " " + output;
		return false;
	}

	const std::string json = ReadFileUtf8(jsonW);
	if (json.empty())
	{
		e.state = ParseState::Failed;
		e.parseError = "Could not read EI JSON.";
		return false;
	}
	e.jsonPathUtf8 = WideToUtf8(jsonW);
	ApplyEiJsonToEntry(e, json);
	return true;
}

DWORD WINAPI ParseWorker(LPVOID)
{
	std::vector<size_t> pending;
	{
		std::lock_guard<std::mutex> lock(gMu);
		for (size_t i = 0; i < gLogs.size(); ++i)
		{
			const LogEntry& e = gLogs[i];
			/* Include uploaded logs that never got EI/dps metadata. */
			const bool needsMeta = e.encounter.empty() || e.result < 0 || e.durationMs <= 0;
			if (e.state == ParseState::Pending || e.state == ParseState::Failed ||
				(needsMeta && e.state != ParseState::Uploading))
				pending.push_back(i);
		}
	}
	gParseTotal.store(static_cast<int>(pending.size()));
	gParseDone.store(0);

	for (size_t idx : pending)
	{
		if (gCancel.load())
			break;
		LogEntry local;
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (idx >= gLogs.size())
				continue;
			local = gLogs[idx];
		}
		ParseOneWithEi(local);
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (idx < gLogs.size() && gLogs[idx].pathUtf8 == local.pathUtf8)
				gLogs[idx] = local;
			gParseDone.fetch_add(1);
			if ((gParseDone.load() % 5) == 0)
				SaveCacheLocked();
			gGen.fetch_add(1);
		}
	}
	{
		std::lock_guard<std::mutex> lock(gMu);
		SaveCacheLocked();
		gGen.fetch_add(1);
	}
	std::snprintf(gStatus, sizeof(gStatus), "Parse finished (%d).", gParseDone.load());
	gParseBusy.store(false);
	return 0;
}

void BeginParsePending()
{
	if (!G::EliteInsightsPath[0])
	{
		std::snprintf(gStatus, sizeof(gStatus),
			"Set Elite Insights CLI path (GuildWars2EliteInsights-CLI.exe).");
		return;
	}
	EiRuntime::InvalidateDotNet8Cache();
	if (!EiRuntime::HasDotNet8Runtime())
	{
		std::snprintf(gStatus, sizeof(gStatus),
			EiRuntime::IsWine()
				? "Install .NET 8 Desktop Runtime into this Wine/Proton prefix first."
				: "Install .NET 8 Desktop Runtime first (button above).");
		return;
	}
	if (gParseBusy.exchange(true))
		return;
	gCancel.store(false);
	std::snprintf(gStatus, sizeof(gStatus), "Parsing with Elite Insights…");
	if (gParseThread)
	{
		CloseHandle(gParseThread);
		gParseThread = nullptr;
	}
	gParseThread = CreateThread(nullptr, 0, ParseWorker, nullptr, 0, nullptr);
	if (!gParseThread)
		gParseBusy.store(false);
}

void BeginParseSelected(const std::string& pathUtf8)
{
	if (pathUtf8.empty())
		return;
	{
		std::lock_guard<std::mutex> lock(gMu);
		for (auto& e : gLogs)
		{
			if (e.pathUtf8 == pathUtf8)
			{
				e.state = ParseState::Pending;
				break;
			}
		}
		gGen.fetch_add(1);
	}
	BeginParsePending();
}

} // namespace LogManagerDetail
