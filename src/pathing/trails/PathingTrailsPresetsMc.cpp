#include "PathingTrails.h"

#include "PathingIndex.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

using namespace PathingDetail;

void PathingTrails::EnableMapCompletionPreset(MapCompletionRoutes routes)
{
	if (routes == MapCompletionRoutes::None)
	{
		ClearMapCompletionCategories();
		return;
	}

	std::lock_guard<std::mutex> lock(gMutex);

	bool hadBroadTwGuides = false;
	for (const std::string& p : gEnabledPaths)
	{
		if (ToLower(p) == "tw_guides")
		{
			hadBroadTwGuides = true;
			break;
		}
	}

	/* Drop prior map-completion enables so we don't stack both editions. */
	gEnabledPaths.erase(
		std::remove_if(gEnabledPaths.begin(), gEnabledPaths.end(),
			[](const std::string& p) { return IsTekkitMcPath(p); }),
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

	/* Prefer DisplayName - SotO/VoE reuse trails/trails2 for Skyscale/Lanterns/Skimmer. */
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
			if (pathLow.find("tw_mc_soto") != std::string::npos ||
				pathLow.find("tw_mc_voe") != std::string::npos)
				return false;
			return ends(leaf, "trails") && !ends(leaf, "trails2") && !ends(leaf, "trails3");
		}
		if (sky)
			return true;
		if (bare || griff || lantern || skimmer)
			return false;
		if (ends(leaf, "trails3") && pathLow.find("tw_mc_eod") == std::string::npos)
			return true;
		if (pathLow.find("tw_mc_soto") != std::string::npos &&
			ends(leaf, "trails") && !ends(leaf, "trails2") && !ends(leaf, "trails3"))
			return true;
		return false;
	};

	auto enableMcPath = [&](const std::string& path)
	{
		if (path.empty())
			return;
		const std::string low = ToLower(path);
		/* Only strip overlapping MC enables - never wipe fishing / me / Lady. */
		gEnabledPaths.erase(
			std::remove_if(gEnabledPaths.begin(), gEnabledPaths.end(),
				[&](const std::string& p) {
					const std::string el = ToLower(p);
					if (!(PrefixMatchesType(el, low) || PrefixMatchesType(low, el)))
						return false;
					return IsTekkitMcPath(el) || IsTekkitMcPath(low) || el == "tw_guides";
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
					enableMcPath(ch.path);
				continue;
			}
			enableMcPath(ch.path);
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
			enableMcPath(list[i]);
	}

	RestoreNonMcTekkitSiblingsLocked(hadBroadTwGuides);

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
		if (ends(leaf, "trails2") &&
			low.find("tw_mc_soto") == std::string::npos &&
			low.find("tw_mc_voe") == std::string::npos)
			bare = true;
		if (ends(leaf, "trails") && !ends(leaf, "trails2") && !ends(leaf, "trails3") &&
			low.find("tw_mc_soto") == std::string::npos &&
			low.find("tw_mc_voe") == std::string::npos &&
			low.find("tw_mc_jw") == std::string::npos)
			griff = true;
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
