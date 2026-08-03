#include "LogManagerPad.h"

#include "LogManagerShared.h"
#include "LogManagerUpload.h"
#include "LogManagerEi.h"

#include "AddonPaths.h"
#include "EiRuntime.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>
#include <shellapi.h>

namespace LogManagerDetail
{

	std::mutex gMu;
	std::vector<LogEntry> gLogs;
	std::atomic<unsigned> gGen{0};
	unsigned gDrawnGen = 0;
	std::vector<LogEntry> gDraw;

	std::atomic<bool> gScanBusy{false};
	std::atomic<bool> gParseBusy{false};
	std::atomic<bool> gUploadBusy{false};
	std::atomic<bool> gHydrateBusy{false};
	std::atomic<bool> gHydrateForce{false};
	std::atomic<bool> gEiInstallBusy{false};
	std::atomic<bool> gCancel{false};
	std::atomic<int> gParseDone{0};
	std::atomic<int> gParseTotal{0};
	std::atomic<int> gUploadDone{0};
	std::atomic<int> gUploadTotal{0};

	HANDLE gScanThread = nullptr;
	HANDLE gParseThread = nullptr;
	HANDLE gUploadThread = nullptr;
	HANDLE gHydrateThread = nullptr;
	HANDLE gEiInstallThread = nullptr;
	HANDLE gKillProofThread = nullptr;

	std::atomic<bool> gKillProofBusy{false};
	std::mutex gKpCacheMu;
	std::unordered_map<std::string, KillProofCacheEntry> gKpCache; /* lowercased account */
	std::vector<std::string> gKpQueue; /* accounts to fetch */
	std::atomic<bool> gKpForce{false};

	bool gFocus = false;
	bool gPlaceOnce = false;
	bool gExpandGroupsOnce = false; /* reopen all encounter sections (screenshot default) */
	char gStatus[256] = {};
	char gEiStatus[256] = {};
	char gSearch[96] = {};
	int gResultFilter = static_cast<int>(ResultFilter::All);
	int gModeFilter = static_cast<int>(ModeFilter::All);
	int gDaysCombo = 0; /* index into kDaysMap */
	int gSelected = -1;
	bool gFocusSetupTab = false;
	float gLogListFrac = kLogListFracDef; /* synced from G::LogManagerListFrac */

	std::vector<std::string> gUploadQueue; /* pathUtf8 */

	std::wstring Utf8ToWide(const char* utf8)
	{
		if (!utf8 || !utf8[0])
			return {};
		const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
		if (n <= 0)
			return {};
		std::wstring out(static_cast<size_t>(n - 1), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), n);
		return out;
	}

	std::string WideToUtf8(const std::wstring& w)
	{
		if (w.empty())
			return {};
		const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (n <= 0)
			return {};
		std::string out(static_cast<size_t>(n - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
		return out;
	}

	bool EndsWithI(const std::wstring& s, const wchar_t* suf)
	{
		const size_t n = std::wcslen(suf);
		if (s.size() < n)
			return false;
		for (size_t i = 0; i < n; ++i)
		{
			wchar_t a = s[s.size() - n + i];
			wchar_t b = suf[i];
			if (a >= L'A' && a <= L'Z') a = static_cast<wchar_t>(a - L'A' + L'a');
			if (b >= L'A' && b <= L'Z') b = static_cast<wchar_t>(b - L'A' + L'a');
			if (a != b)
				return false;
		}
		return true;
	}

	std::wstring DefaultLogDirW()
	{
		wchar_t profile[MAX_PATH]{};
		const DWORD n = GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
		if (n == 0 || n >= MAX_PATH)
			return {};
		return std::wstring(profile) + L"\\Documents\\Guild Wars 2\\addons\\arcdps\\arcdps.cbtlogs";
	}

	void EnsureDefaultPaths()
	{
		if (!G::LogFolder[0])
		{
			const std::string def = WideToUtf8(DefaultLogDirW());
			if (!def.empty())
				std::snprintf(G::LogFolder, sizeof(G::LogFolder), "%s", def.c_str());
		}
	}

	bool PathExistsUtf8(const char* utf8)
	{
		if (!utf8 || !utf8[0])
			return false;
		const std::wstring w = Utf8ToWide(utf8);
		if (w.empty())
			return false;
		return GetFileAttributesW(w.c_str()) != INVALID_FILE_ATTRIBUTES;
	}

	bool IsManagedEiPath(const char* path)
	{
		if (!path || !path[0])
			return false;
		const std::string data = AddonPaths::DataDirUtf8();
		if (data.empty())
			return false;
		std::string needle = data;
		for (char& c : needle)
			if (c == '/')
				c = '\\';
		std::string p = path;
		for (char& c : p)
			if (c == '/')
				c = '\\';
		/* case-insensitive contains "\ei\" under addon data dir */
		auto lower = [](std::string s) {
			for (char& c : s)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return s;
		};
		const std::string pl = lower(p);
		const std::string dl = lower(needle);
		if (pl.find(dl) == std::string::npos)
			return false;
		return pl.find("\\ei\\") != std::string::npos ||
			pl.find("\\ei/") != std::string::npos;
	}

	bool ApplyManagedCliPath()
	{
		char cli[512]{};
		if (!EiRuntime::GetCliPathUtf8(AddonPaths::DataDir().c_str(), cli, sizeof(cli)))
			return false;
		if (!G::EliteInsightsPath[0] || IsManagedEiPath(G::EliteInsightsPath) ||
			!PathExistsUtf8(G::EliteInsightsPath))
		{
			std::snprintf(G::EliteInsightsPath, sizeof(G::EliteInsightsPath), "%s", cli);
			Settings::SetDirty();
		}
		return true;
	}

	time_t FileTimeToUnix(const FILETIME& ft)
	{
		ULARGE_INTEGER u;
		u.LowPart = ft.dwLowDateTime;
		u.HighPart = ft.dwHighDateTime;
		/* FILETIME is 100ns since 1601; Unix is seconds since 1970 */
		constexpr ULONGLONG kEpochDiff = 116444736000000000ULL;
		if (u.QuadPart < kEpochDiff)
			return 0;
		return static_cast<time_t>((u.QuadPart - kEpochDiff) / 10000000ULL);
	}

	std::string FmtDuration(long long ms)
	{
		if (ms <= 0)
			return "-";
		const long long totalSec = ms / 1000;
		const int h = static_cast<int>(totalSec / 3600);
		const int m = static_cast<int>((totalSec % 3600) / 60);
		const int s = static_cast<int>(totalSec % 60);
		char buf[32];
		if (h > 0)
			std::snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
		else
			std::snprintf(buf, sizeof(buf), "%d:%02d", m, s);
		return buf;
	}

	std::string FmtTime(time_t t)
	{
		if (t <= 0)
			return "-";
		const std::tm* tm = std::localtime(&t);
		if (!tm)
			return "-";
		char buf[40];
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm);
		return buf;
	}


	bool ContainsI(const std::string& hay, const char* needle)
	{
		if (!needle || !needle[0])
			return true;
		auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
		std::string h = hay;
		std::string n = needle;
		std::transform(h.begin(), h.end(), h.begin(), lower);
		std::transform(n.begin(), n.end(), n.begin(), lower);
		return h.find(n) != std::string::npos;
	}

	bool CopyText(const char* text)
	{
		if (!text || !text[0])
			return false;
		const size_t n = std::strlen(text) + 1;
		if (!OpenClipboard(nullptr))
			return false;
		EmptyClipboard();
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, n);
		if (!mem)
		{
			CloseClipboard();
			return false;
		}
		void* p = GlobalLock(mem);
		if (!p)
		{
			GlobalFree(mem);
			CloseClipboard();
			return false;
		}
		std::memcpy(p, text, n);
		GlobalUnlock(mem);
		SetClipboardData(CF_TEXT, mem);
		CloseClipboard();
		return true;
	}

	const char* ResultLabel(int r)
	{
		if (r == 1)
			return "Kill";
		if (r == 0)
			return "Fail";
		return "?";
	}

	void SyncDraw()
	{
		const unsigned gen = gGen.load();
		if (gen == gDrawnGen)
			return;
		std::lock_guard<std::mutex> lock(gMu);
		gDraw = gLogs;
		gDrawnGen = gGen.load();
	}


	void OpenFolderFor(const std::wstring& path)
	{
		std::wstring dir = path;
		const auto slash = dir.find_last_of(L"\\/");
		if (slash != std::wstring::npos)
			dir.resize(slash);
		ShellExecuteW(nullptr, L"explore", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	}

	void CollectFiltered(std::vector<const LogEntry*>& filtered)
	{
		filtered.clear();
		filtered.reserve(gDraw.size());
		if (gDaysCombo < 0 || gDaysCombo > 4)
			gDaysCombo = 0;
		const int daysCut = kDaysMap[gDaysCombo];
		const time_t now = std::time(nullptr);
		const time_t cut = daysCut > 0 ? now - static_cast<time_t>(daysCut) * 86400 : 0;
		for (const LogEntry& e : gDraw)
		{
			if (gSearch[0] && !ContainsI(e.fileName, gSearch) && !ContainsI(e.encounter, gSearch) &&
				!ContainsI(e.pathUtf8, gSearch))
				continue;
			const auto rf = static_cast<ResultFilter>(gResultFilter);
			if (rf == ResultFilter::Success && e.result != 1)
				continue;
			if (rf == ResultFilter::Failure && e.result != 0)
				continue;
			if (rf == ResultFilter::Unknown && e.result != -1)
				continue;
			const auto mf = static_cast<ModeFilter>(gModeFilter);
			if (mf == ModeFilter::Normal && !e.mode.empty())
				continue;
			if (mf == ModeFilter::CM && e.mode != "CM")
				continue;
			if (mf == ModeFilter::LCM && e.mode != "LCM")
				continue;
			if (cut > 0)
			{
				const time_t t = e.encounterTime > 0 ? e.encounterTime : FileTimeToUnix(e.mtime);
				if (t < cut)
					continue;
			}
			filtered.push_back(&e);
		}
	}

} // namespace LogManagerDetail

using namespace LogManagerDetail;

void LogManagerPad::OpenAndRefresh()
{
	G::ShowLogManager = true;
	gFocus = true;
	gPlaceOnce = true;
	gExpandGroupsOnce = true;
	gLogListFrac = G::LogManagerListFrac;
	EnsureDefaultPaths();
	Settings::SetDirty();
	BeginEiEnsure(false);
	BeginScan();
}

bool LogManagerPad::Render()
{
	if (!G::ShowLogManager)
		return false;

	EnsureDefaultPaths();
	SyncDraw();

	const ImVec2 display = ImGui::GetIO().DisplaySize;
	const float displayW = display.x > 1.f ? display.x : kPadW;
	const float displayH = display.y > 1.f ? display.y : kPadH;

	if (gPlaceOnce)
	{
		float winW = G::LogManagerWinW;
		float winH = G::LogManagerWinH;
		/* First open (no saved pos): nearly full client — screenshot three-pane fit. */
		if (G::LogManagerWinX < 0.f || G::LogManagerWinY < 0.f)
		{
			winW = displayW * 0.92f;
			winH = displayH * 0.84f;
			if (winW < 1100.f && displayW >= 1200.f) winW = 1100.f;
			if (winH < 620.f && displayH >= 720.f) winH = 620.f;
			if (winW > 2200.f) winW = 2200.f;
			if (winH > 1200.f) winH = 1200.f;
		}
		/* Always clamp to current display. */
		{
			const float maxW = displayW > 80.f ? displayW - 24.f : winW;
			const float maxH = displayH > 100.f ? displayH - 48.f : winH;
			if (winW > maxW) winW = maxW;
			if (winH > maxH) winH = maxH;
			if (winW < 880.f && displayW > 920.f) winW = 880.f;
			if (winH < 420.f && displayH > 480.f) winH = 420.f;
			if (winW > displayW * 0.98f) winW = displayW * 0.98f;
			if (winH > displayH * 0.95f) winH = displayH * 0.95f;
		}
		G::LogManagerWinW = winW;
		G::LogManagerWinH = winH;
		if (G::LogManagerWinX >= 0.f && G::LogManagerWinY >= 0.f)
			ImGui::SetNextWindowPos(ImVec2(G::LogManagerWinX, G::LogManagerWinY), ImGuiCond_Always);
		else
			ImGui::SetNextWindowPos(ImVec2(24.f, 36.f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
		gLogListFrac = G::LogManagerListFrac;
		gPlaceOnce = false;
	}

	{
		float minW = 1000.f;
		float minH = 520.f;
		if (minW > displayW * 0.92f) minW = displayW * 0.92f;
		if (minH > displayH * 0.85f) minH = displayH * 0.85f;
		ImGui::SetNextWindowSizeConstraints(
			ImVec2(minW, minH),
			ImVec2(displayW * 0.98f, displayH * 0.95f));
	}
	if (gFocus)
	{
		ImGui::SetNextWindowFocus();
		gFocus = false;
	}

	bool open = G::ShowLogManager;
	HelperTheme::ScopedWindow theme(G::Opacity);
	if (!ImGui::Begin("DPS Logs###gw2igh_logmgr", &open, ImGuiWindowFlags_None))
	{
		ImGui::End();
		if (!open)
		{
			G::ShowLogManager = false;
			Settings::SetDirty();
		}
		return false;
	}
	if (!open)
	{
		G::ShowLogManager = false;
		Settings::SetDirty();
		ImGui::End();
		return false;
	}

	HelperTheme::ScopedFontScale fontScale(1200.f, 800.f);

	const bool hasDotNet = EiRuntime::HasDotNet8Runtime();
	MaybeAutoParseAfterScan(hasDotNet);
	std::vector<const LogEntry*> filtered;
	CollectFiltered(filtered);

	DrawToolbar(filtered, hasDotNet);
	ImGui::Separator();

	const float bodyH = ImGui::GetContentRegionAvail().y;
	const float bodyW = ImGui::GetContentRegionAvail().x;
	float filterW = bodyW * kFilterFrac;
	if (filterW < kFilterMinW) filterW = kFilterMinW;
	if (filterW > kFilterMaxW) filterW = kFilterMaxW;
	/* Narrow / 1080p: keep filters readable — radios need ~210px. */
	if (bodyW < 1100.f)
	{
		filterW = bodyW * 0.20f;
		if (filterW < 210.f) filterW = 210.f;
		if (filterW > 250.f) filterW = 250.f;
	}
	if (filterW > bodyW * 0.28f)
		filterW = bodyW * 0.28f;

	ImGui::BeginChild("###gw2igh_lm_filters", ImVec2(filterW, bodyH), true);
	DrawFilterPane();
	ImGui::EndChild();

	ImGui::SameLine(0.f, kPaneGap);
	/* Log list | drag splitter | detail — fraction of remaining width; tables stretch inside. */
	const float availX = ImGui::GetContentRegionAvail().x;
	const float usable = availX - kSplitHitW - kPaneGap * 2.f;
	float listMin = kLogListMinW;
	float rightMin = kRightPaneMinW;
	if (usable > 1.f && usable < listMin + rightMin)
	{
		const float scale = usable / (listMin + rightMin);
		listMin *= scale;
		rightMin *= scale;
	}
	if (gLogListFrac < 0.20f)
		gLogListFrac = 0.20f;
	if (gLogListFrac > 0.72f)
		gLogListFrac = 0.72f;
	float centerW = usable * gLogListFrac;
	if (centerW < listMin)
		centerW = listMin;
	if (centerW > usable - rightMin)
		centerW = usable - rightMin;
	if (centerW < listMin)
		centerW = listMin;
	if (usable > 1.f)
		gLogListFrac = centerW / usable;

	ImGui::BeginChild("###gw2igh_lm_list", ImVec2(centerW, bodyH), true);
	DrawLogTable(filtered);
	ImGui::EndChild();

	ImGui::SameLine(0.f, kPaneGap);
	{
		const ImVec2 splitPos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("###gw2igh_lm_split", ImVec2(kSplitHitW, bodyH));
		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();
		if (hovered || active)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		if (active && usable > 1.f)
		{
			centerW += ImGui::GetIO().MouseDelta.x;
			if (centerW < listMin)
				centerW = listMin;
			if (centerW > usable - rightMin)
				centerW = usable - rightMin;
			gLogListFrac = centerW / usable;
			if (std::fabs(G::LogManagerListFrac - gLogListFrac) > 0.002f)
			{
				G::LogManagerListFrac = gLogListFrac;
				Settings::SetDirty();
			}
		}
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float midX = splitPos.x + kSplitHitW * 0.5f;
		const ImU32 col = ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive
			: (hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
		dl->AddLine(ImVec2(midX, splitPos.y + 4.f),
			ImVec2(midX, splitPos.y + bodyH - 4.f), col, active ? 2.f : 1.f);
		if (hovered)
			ImGui::SetTooltip("Drag to resize panes — tables scale with width");
	}

	ImGui::SameLine(0.f, kPaneGap);
	ImGui::BeginChild("###gw2igh_lm_side", ImVec2(0.f, bodyH), true);
	if (ImGui::BeginTabBar("###gw2igh_lm_tabs", ImGuiTabBarFlags_FittingPolicyScroll))
	{
		if (ImGui::BeginTabItem("Detail"))
		{
			DrawDetailTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Players"))
		{
			DrawPlayersTab(filtered);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("KillProof"))
		{
			DrawKillProofTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Guilds"))
		{
			DrawGuildsTab(filtered);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Fastest"))
		{
			DrawFastestTab(filtered);
			ImGui::EndTabItem();
		}
		{
			ImGuiTabItemFlags setupFlags = 0;
			if (gFocusSetupTab)
			{
				setupFlags = ImGuiTabItemFlags_SetSelected;
				gFocusSetupTab = false;
			}
			if (ImGui::BeginTabItem("Setup", nullptr, setupFlags))
			{
				DrawSetupTab(hasDotNet);
				ImGui::EndTabItem();
			}
		}
		ImGui::EndTabBar();
	}
	ImGui::EndChild();

	{
		const ImVec2 pos = ImGui::GetWindowPos();
		const ImVec2 sz = ImGui::GetWindowSize();
		const bool moved =
			std::fabs(pos.x - G::LogManagerWinX) > 0.5f ||
			std::fabs(pos.y - G::LogManagerWinY) > 0.5f ||
			std::fabs(sz.x - G::LogManagerWinW) > 0.5f ||
			std::fabs(sz.y - G::LogManagerWinH) > 0.5f;
		if (moved)
		{
			G::LogManagerWinX = pos.x;
			G::LogManagerWinY = pos.y;
			G::LogManagerWinW = sz.x;
			G::LogManagerWinH = sz.y;
			Settings::SetDirty();
		}
	}

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
		ImGuiHoveredFlags_AllowWhenBlockedByPopup);
	ImGui::End();
	return hovered;
}
