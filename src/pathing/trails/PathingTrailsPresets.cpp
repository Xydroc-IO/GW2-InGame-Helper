#include "PathingTrails.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "PathingIndex.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>
#include <shellapi.h>

using namespace PathingDetail;

namespace PathingDetail
{
	void EnsureRootEnabledLocked(const char* root)
	{
		if (!root || !root[0])
			return;
		const std::string low = ToLower(root);
		gEnabledPaths.erase(
			std::remove_if(gEnabledPaths.begin(), gEnabledPaths.end(),
				[&](const std::string& p) {
					const std::string el = ToLower(p);
					return el != low && PrefixMatchesType(el, low);
				}),
			gEnabledPaths.end());
		for (const std::string& p : gEnabledPaths)
		{
			const std::string el = ToLower(p);
			if (el == low || PrefixMatchesType(low, el))
				return;
		}
		gEnabledPaths.push_back(root);
	}

	bool IsTekkitMcPath(const std::string& p)
	{
		const std::string low = ToLower(p);
		if (low == "tw_guides.tw_mc")
			return true;
		if (low.size() >= 16 && low.compare(0, 16, "tw_guides.tw_mc.") == 0)
			return true;
		if (low.size() >= 16 && low.compare(0, 16, "tw_guides.tw_mc_") == 0)
			return true;
		return false;
	}

	void RestoreNonMcTekkitSiblingsLocked(bool hadBroadTwGuides)
	{
		if (!hadBroadTwGuides)
			return;

		gEnabledPaths.erase(
			std::remove_if(gEnabledPaths.begin(), gEnabledPaths.end(),
				[](const std::string& p) { return ToLower(p) == "tw_guides"; }),
			gEnabledPaths.end());

		std::function<void(const std::vector<PathingTrails::Category>&)> walk =
			[&](const std::vector<PathingTrails::Category>& nodes)
		{
			for (const PathingTrails::Category& n : nodes)
			{
				if (ToLower(n.path) == "tw_guides")
				{
					for (const PathingTrails::Category& ch : n.children)
					{
						if (ch.hidden || ch.separator)
							continue;
						if (IsTekkitMcPath(ch.path))
							continue;
						EnsureRootEnabledLocked(ch.path.c_str());
					}
					return;
				}
				if (!n.children.empty())
					walk(n.children);
			}
		};
		if (!gMenu.empty())
			walk(gMenu);
		else
		{
			static const char* kSiblings[] = {
				"tw_guides.tw_fishing",
				"tw_guides.tw_gm",
				"tw_guides.tw_me",
				"tw_guides.tw_jp",
				"tw_guides.tw_fractals",
				"tw_guides.tw_gatheringnodes",
				"tw_guides.tw_festivals",
				"tw_guides.tw_core_achievements",
				"tw_guides.tw_beetleraces",
				"tw_guides.tw_blkf",
				"tw_guides.tw_sidestories",
				"tw_guides.tw_pactsupplynetwork",
				"tw_guides.tw_hot",
				"tw_guides.tw_pof",
				"tw_guides.tw_eod",
				"tw_guides.tw_soto",
				"tw_guides.tw_jw",
				"tw_guides.tw_voe",
				"tw_guides.tw_lws1",
				"tw_guides.tw_lws2",
				"tw_guides.tw_lws3",
				"tw_guides.tw_lws4",
				"tw_guides.tw_lws5",
				"tw_guides.tw_mp",
				"tw_guides.tw_collections_legendarytrinkets",
			};
			for (const char* s : kSiblings)
				EnsureRootEnabledLocked(s);
		}
	}
} // namespace PathingDetail

void PathingTrails::ClearMapCompletionCategories()
{
	std::lock_guard<std::mutex> lock(gMutex);

	gEnabledPaths.erase(
		std::remove_if(gEnabledPaths.begin(), gEnabledPaths.end(),
			[](const std::string& p) { return IsTekkitMcPath(p); }),
		gEnabledPaths.end());
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

void PathingTrails::EnableAllTekkitCategories()
{
	std::lock_guard<std::mutex> lock(gMutex);
	EnsureRootEnabledLocked("tw_guides");
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

void PathingTrails::EnableAllLadyCategories()
{
	std::lock_guard<std::mutex> lock(gMutex);
	EnsureRootEnabledLocked("legs");
	EnsureRootEnabledLocked("leag");
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

void PathingTrails::NotifyVisibilityFilterChanged()
{
	gContentRevision.fetch_add(1, std::memory_order_release);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

void PathingTrails::EnableAllHeroCategories()
{
	std::lock_guard<std::mutex> lock(gMutex);
	EnsureRootEnabledLocked("HMP");
	EnsureRootEnabledLocked("hmpSim");
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

void PathingTrails::DisableAllCategories()
{
	std::lock_guard<std::mutex> lock(gMutex);
	gEnabledPaths.clear();
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

std::vector<std::string> PathingTrails::EnabledPaths()
{
	std::lock_guard<std::mutex> lock(gMutex);
	return gEnabledPaths;
}

void PathingTrails::SetEnabledPaths(const std::vector<std::string>& paths)
{
	std::lock_guard<std::mutex> lock(gMutex);
	gEnabledPaths = paths;
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

void PathingTrails::SerializeEnabledPaths(char* out, size_t outLen)
{
	if (!out || outLen == 0)
		return;
	out[0] = 0;
	const std::vector<std::string> paths = EnabledPaths();
	size_t used = 0;
	for (size_t i = 0; i < paths.size(); ++i)
	{
		const std::string& p = paths[i];
		if (p.empty())
			continue;
		const size_t need = p.size() + (used ? 1u : 0u);
		if (used + need + 1 >= outLen)
			break;
		if (used)
			out[used++] = '|';
		std::memcpy(out + used, p.c_str(), p.size());
		used += p.size();
		out[used] = 0;
	}
}

void PathingTrails::ParseEnabledPaths(const char* pipeList)
{
	std::vector<std::string> paths;
	if (pipeList && pipeList[0])
	{
		std::string cur;
		for (const char* p = pipeList; ; ++p)
		{
			const char c = *p;
			if (c == '|' || c == ',' || c == '\n' || c == '\r' || c == 0)
			{
				while (!cur.empty() && (cur.back() == ' ' || cur.back() == '\t'))
					cur.pop_back();
				size_t start = 0;
				while (start < cur.size() && (cur[start] == ' ' || cur[start] == '\t'))
					++start;
				if (start < cur.size())
					paths.push_back(cur.substr(start));
				cur.clear();
				if (c == 0)
					break;
				continue;
			}
			cur.push_back(c);
		}
	}
	SetEnabledPaths(paths);
}

bool PathingTrails::OpenPathingFolder()
{
	const std::string hint = PathingFolderHint();
	if (hint.empty())
		return false;
	wchar_t wpath[MAX_PATH]{};
	if (MultiByteToWideChar(CP_UTF8, 0, hint.c_str(), -1, wpath, MAX_PATH) <= 0)
		return false;
	CreateDirectoryW(wpath, nullptr);
	const HINSTANCE r = ShellExecuteW(nullptr, L"explore", wpath, nullptr, nullptr, SW_SHOWNORMAL);
	return reinterpret_cast<INT_PTR>(r) > 32;
}
