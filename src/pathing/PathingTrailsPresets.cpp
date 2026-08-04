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

void PathingTrails::EnableMapCompletionPreset(MapCompletionRoutes routes)
{
	if (routes == MapCompletionRoutes::None)
	{
		ClearMapCompletionCategories();
		return;
	}

	std::lock_guard<std::mutex> lock(gMutex);

	/* Match Core + every expansion (tw_mc_hot, tw_mc_pof, …) — PrefixMatchesType
	   on "tw_guides.tw_mc" alone misses tw_mc_hot because '_' ≠ '.'. */
	auto isMcPath = [](const std::string& p) -> bool
	{
		const std::string low = ToLower(p);
		if (low == "tw_guides.tw_mc")
			return true;
		if (low.size() >= 16 && low.compare(0, 16, "tw_guides.tw_mc.") == 0)
			return true;
		if (low.size() >= 16 && low.compare(0, 16, "tw_guides.tw_mc_") == 0)
			return true;
		return false;
	};

	/* Drop prior map-completion enables so we don't stack both editions. */
	gEnabledPaths.erase(
		std::remove_if(gEnabledPaths.begin(), gEnabledPaths.end(),
			[&](const std::string& p) { return isMcPath(p); }),
		gEnabledPaths.end());

	auto leafOf = [](const std::string& path) -> std::string
	{
		const size_t dot = path.find_last_of('.');
		return (dot == std::string::npos) ? path : path.substr(dot + 1);
	};

	auto isRouteFolder = [&](const Category& c) -> bool
	{
		const std::string leaf = ToLower(leafOf(c.path));
		const std::string lab = ToLower(c.label);
		if (leaf.find("trails") != std::string::npos)
			return true;
		if (lab.find("routes") != std::string::npos)
			return true;
		if (lab.find("edition") != std::string::npos)
			return true;
		return false;
	};

	/* Prefer DisplayName — SotO/VoE reuse trails/trails2 for Skyscale/Lanterns/Skimmer. */
	auto matchesRoutes = [&](const Category& c) -> bool
	{
		const std::string leaf = ToLower(leafOf(c.path));
		const std::string lab = ToLower(c.label);
		const std::string pathLow = ToLower(c.path);

		auto ends = [](const std::string& s, const char* suf) -> bool
		{
			const size_t n = std::strlen(suf);
			return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
		};

		/* Single generic "Routes" (e.g. Janthir Wilds) — OK for any pick. */
		if (lab == "routes")
			return true;

		const bool bare = lab.find("barefoot") != std::string::npos;
		const bool griff = lab.find("griffon") != std::string::npos;
		const bool sky = lab.find("skyscale") != std::string::npos;
		const bool lantern = lab.find("lantern") != std::string::npos;
		const bool skimmer = lab.find("skimmer") != std::string::npos;

		if (routes == MapCompletionRoutes::Barefoot)
		{
			if (bare)
				return true;
			if (griff || sky || lantern || skimmer)
				return false;
			return ends(leaf, "trails2");
		}
		if (routes == MapCompletionRoutes::Griffon)
		{
			if (griff)
				return true;
			if (bare || sky || lantern || skimmer)
				return false;
			/* Exclude SotO/VoE — their primary trails folder is Skyscale/Skimmer. */
			if (pathLow.find("tw_mc_soto") != std::string::npos ||
				pathLow.find("tw_mc_voe") != std::string::npos)
				return false;
			return ends(leaf, "trails") && !ends(leaf, "trails2") && !ends(leaf, "trails3");
		}
		/* Skyscale: HoT trails3, SotO trails (Skyscale Edition), label match. */
		if (sky)
			return true;
		if (bare || griff || lantern || skimmer)
			return false;
		if (ends(leaf, "trails3") && pathLow.find("tw_mc_eod") == std::string::npos)
			return true; /* HoT Skyscale; skip EoD lanterns trails3 */
		if (pathLow.find("tw_mc_soto") != std::string::npos &&
			ends(leaf, "trails") && !ends(leaf, "trails2") && !ends(leaf, "trails3"))
			return true;
		return false;
	};

	auto enablePath = [&](const std::string& path)
	{
		if (path.empty())
			return;
		const std::string low = ToLower(path);
		gEnabledPaths.erase(
			std::remove_if(gEnabledPaths.begin(), gEnabledPaths.end(),
				[&](const std::string& p) {
					const std::string el = ToLower(p);
					return PrefixMatchesType(el, low) || PrefixMatchesType(low, el);
				}),
			gEnabledPaths.end());
		gEnabledPaths.push_back(path);
	};

	std::function<void(const Category&)> visitExpansion = [&](const Category& node)
	{
		for (const Category& ch : node.children)
		{
			if (ch.hidden)
				continue;
			if (ch.separator)
			{
				visitExpansion(ch);
				continue;
			}
			if (isRouteFolder(ch))
			{
				if (matchesRoutes(ch))
					enablePath(ch.path);
				continue;
			}
			/* Hearts, POIs, vistas, waypoints, hero points, etc. */
			enablePath(ch.path);
		}
	};

	auto isExpansionRoot = [](const std::string& path) -> bool
	{
		static const char* roots[] = {
			"tw_guides.tw_mc", "tw_guides.tw_mc_hot", "tw_guides.tw_mc_pof",
			"tw_guides.tw_mc_eod", "tw_guides.tw_mc_soto", "tw_guides.tw_mc_jw",
			"tw_guides.tw_mc_voe", "tw_guides.tw_mc_lws3",
			"tw_guides.tw_mc_lws4", "tw_guides.tw_mc_lws5",
		};
		for (const char* r : roots)
			if (ToLower(path) == r)
				return true;
		return false;
	};

	std::function<void(const std::vector<Category>&)> walk = [&](const std::vector<Category>& nodes)
	{
		for (const Category& n : nodes)
		{
			if (isExpansionRoot(n.path))
				visitExpansion(n);
			if (!n.children.empty())
				walk(n.children);
		}
	};

	if (!gMenu.empty())
	{
		walk(gMenu);
	}
	else
	{
		/* Pack menu not indexed yet — enable known route folders only. */
		static const char* bareRoutes[] = {
			"tw_guides.tw_mc.tw_mc_trails2",
			"tw_guides.tw_mc_hot.tw_mc_hot_trails2",
			"tw_guides.tw_mc_pof.tw_mc_pof_trails2",
			"tw_guides.tw_mc_eod.tw_mc_eod_trails2",
			"tw_guides.tw_mc_lws3.tw_mc_lws3_trails2",
			"tw_guides.tw_mc_lws4.tw_mc_lws4_trails2",
			"tw_guides.tw_mc_lws5.tw_mc_lws5_trails2",
		};
		static const char* griffRoutes[] = {
			"tw_guides.tw_mc.tw_mc_trails",
			"tw_guides.tw_mc_hot.tw_mc_hot_trails",
			"tw_guides.tw_mc_pof.tw_mc_pof_trails",
			"tw_guides.tw_mc_eod.tw_mc_eod_trails",
			"tw_guides.tw_mc_lws3.tw_mc_lws3_trails",
			"tw_guides.tw_mc_lws4.tw_mc_lws4_trails",
			"tw_guides.tw_mc_lws5.tw_mc_lws5_trails",
		};
		static const char* skyRoutes[] = {
			"tw_guides.tw_mc_hot.tw_mc_hot_trails3",
			"tw_guides.tw_mc_soto.tw_mc_soto_trails",
			"tw_guides.tw_mc_jw.tw_mc_jw_trails",
		};
		const char** list = bareRoutes;
		size_t n = sizeof(bareRoutes) / sizeof(bareRoutes[0]);
		if (routes == MapCompletionRoutes::Griffon)
		{
			list = griffRoutes;
			n = sizeof(griffRoutes) / sizeof(griffRoutes[0]);
		}
		else if (routes == MapCompletionRoutes::Skyscale)
		{
			list = skyRoutes;
			n = sizeof(skyRoutes) / sizeof(skyRoutes[0]);
		}
		for (size_t i = 0; i < n; ++i)
			enablePath(list[i]);
	}

	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

void PathingTrails::ClearMapCompletionCategories()
{
	std::lock_guard<std::mutex> lock(gMutex);

	auto isMcPath = [](const std::string& p) -> bool
	{
		const std::string low = ToLower(p);
		if (low == "tw_guides.tw_mc")
			return true;
		if (low.size() >= 16 && low.compare(0, 16, "tw_guides.tw_mc.") == 0)
			return true;
		if (low.size() >= 16 && low.compare(0, 16, "tw_guides.tw_mc_") == 0)
			return true;
		return false;
	};

	gEnabledPaths.erase(
		std::remove_if(gEnabledPaths.begin(), gEnabledPaths.end(),
			[&](const std::string& p) { return isMcPath(p); }),
		gEnabledPaths.end());
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

PathingTrails::MapCompletionRoutes PathingTrails::ActiveMapCompletionRoutes()
{
	std::lock_guard<std::mutex> lock(gMutex);
	bool bare = false;
	bool griff = false;
	bool sky = false;
	for (const std::string& p : gEnabledPaths)
	{
		const std::string low = ToLower(p);
		const size_t dot = low.find_last_of('.');
		const std::string leaf = (dot == std::string::npos) ? low : low.substr(dot + 1);
		auto ends = [](const std::string& s, const char* suf) -> bool
		{
			const size_t n = std::strlen(suf);
			return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
		};
		/* Known Barefoot / Griffon / Skyscale folders (not SotO lanterns / VoE skimmer). */
		if (ends(leaf, "trails2") &&
			low.find("tw_mc_soto") == std::string::npos &&
			low.find("tw_mc_voe") == std::string::npos)
			bare = true;
		if (ends(leaf, "trails") && !ends(leaf, "trails2") && !ends(leaf, "trails3") &&
			low.find("tw_mc_soto") == std::string::npos &&
			low.find("tw_mc_voe") == std::string::npos &&
			low.find("tw_mc_jw") == std::string::npos)
			griff = true;
		/* HoT Skyscale trails3; SotO primary trails = Skyscale Edition. */
		if (ends(leaf, "trails3") && low.find("tw_mc_eod") == std::string::npos)
			sky = true;
		if (low.find("tw_mc_soto") != std::string::npos &&
			ends(leaf, "trails") && !ends(leaf, "trails2") && !ends(leaf, "trails3"))
			sky = true;
	}
	const int n = (bare ? 1 : 0) + (griff ? 1 : 0) + (sky ? 1 : 0);
	if (n != 1)
		return MapCompletionRoutes::None;
	if (bare)
		return MapCompletionRoutes::Barefoot;
	if (griff)
		return MapCompletionRoutes::Griffon;
	return MapCompletionRoutes::Skyscale;
}

void PathingTrails::EnableAllTekkitCategories()
{
	std::lock_guard<std::mutex> lock(gMutex);
	gEnabledPaths.clear();
	gEnabledPaths.push_back("tw_guides");
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

void PathingTrails::EnableAllLadyCategories()
{
	std::lock_guard<std::mutex> lock(gMutex);
	gEnabledPaths.clear();
	gEnabledPaths.push_back("legs"); /* Lady Elyssa's Guides */
	gEnabledPaths.push_back("leag"); /* Lady Elyssa's AP Guides */
	MarkEnabled(gMenu);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	gEnabledGen.fetch_add(1, std::memory_order_release);
	gForceReload.store(true, std::memory_order_release);
}

void PathingTrails::NotifyVisibilityFilterChanged()
{
	gContentRevision.fetch_add(1, std::memory_order_release);
	gMenuRevision.fetch_add(1, std::memory_order_release);
	/* Re-rank pack load so WP / Hearts / HP train are not starved by Barefoot fill. */
	gForceReload.store(true, std::memory_order_release);
}

void PathingTrails::EnableAllHeroCategories()
{
	std::lock_guard<std::mutex> lock(gMutex);
	gEnabledPaths.clear();
	gEnabledPaths.push_back("HMP");
	gEnabledPaths.push_back("hmpSim");
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
	/* Ensure folder exists so Explorer has somewhere to land. */
	wchar_t wpath[MAX_PATH]{};
	if (MultiByteToWideChar(CP_UTF8, 0, hint.c_str(), -1, wpath, MAX_PATH) <= 0)
		return false;
	CreateDirectoryW(wpath, nullptr);
	const HINSTANCE r = ShellExecuteW(nullptr, L"explore", wpath, nullptr, nullptr, SW_SHOWNORMAL);
	return reinterpret_cast<INT_PTR>(r) > 32;
}
