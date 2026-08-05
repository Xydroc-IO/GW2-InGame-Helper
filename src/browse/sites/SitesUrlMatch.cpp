#include "Sites.h"
#include "SitesInternal.h"

#include <cstring>
#include <string>

using SitesDetail::gSites;
using SitesDetail::gSiteCount;
using SitesRuntimeDetail::EnsureUrlKeys;
using SitesRuntimeDetail::gExactBuiltin;
using SitesRuntimeDetail::gUrlKeys;
using SitesRuntimeDetail::gUrlKeysByHost;
using SitesRuntimeDetail::gUrlKeysReady;
using SitesRuntimeDetail::SiteUrlKey;
using SitesRuntimeDetail::StartUrlKeysBuild;
using SitesRuntimeDetail::TickUrlKeysBuild;
using SitesRuntimeDetail::UrlHostPath;

void Sites::WarmUrlKeys()
{
	/* Kick off only — do not build all ~2600 keys on AddonLoad. */
	StartUrlKeysBuild();
	TickUrlKeysBuild(64);
}

bool Sites::TickWarmUrlKeys(int sitesPerTick)
{
	if (gUrlKeysReady)
		return true;
	TickUrlKeysBuild(sitesPerTick > 0 ? sitesPerTick : 128);
	return gUrlKeysReady;
}

bool Sites::UrlKeysReady()
{
	return gUrlKeysReady;
}

int Sites::BestMatchForUrl(const std::string& url)
{
	if (url.empty())
		return -1;

	if (url.rfind("about:", 0) == 0 || url.rfind("file:", 0) == 0)
	{
		if (gUrlKeysReady)
		{
			const auto exact = gExactBuiltin.find(url);
			if (exact != gExactBuiltin.end())
				return exact->second;
		}
		else
			EnsureUrlKeys(); /* one chunk only */

		/* file:///…/helper-home.html etc. (resolved paths, not about: keys). */
		auto fileHit = [&](const char* needle, const char* id) -> int {
			if (url.find(needle) != std::string::npos)
				return IndexOfId(id);
			return -1;
		};
		int hit = fileHit("live-browse-hub", "browse");
		if (hit >= 0) return hit;
		hit = fileHit("helper-home", "home");
		if (hit >= 0) return hit;
		hit = fileHit("dps-log-setup", "dpsloghelp");
		if (hit >= 0) return hit;
		hit = fileHit("api-key-setup", "apikeyhelp");
		if (hit >= 0) return hit;
		hit = fileHit("raid-food", "raidfood");
		if (hit >= 0) return hit;
		hit = fileHit("ubers-all-in-one", "ubersaio");
		if (hit >= 0) return hit;
		hit = fileHit("raid-utilities", "raidutils");
		if (hit >= 0) return hit;
		hit = fileHit("fractal-consumables", "fractalcons");
		if (hit >= 0) return hit;
		hit = fileHit("sigils-runes", "sigilsrunes");
		if (hit >= 0) return hit;
		hit = fileHit("relics-guide", "relics");
		if (hit >= 0) return hit;
		hit = fileHit("boon-checklist", "booncheck");
		if (hit >= 0) return hit;
		hit = fileHit("cc-defiance", "ccdefiance");
		if (hit >= 0) return hit;
		hit = fileHit("raid-wings", "raidwings");
		if (hit >= 0) return hit;
		hit = fileHit("home-garden", "homegarden");
		if (hit >= 0) return hit;
		hit = fileHit("strike-missions", "strikes");
		if (hit >= 0) return hit;
		hit = fileHit("fractal-cm-list", "fractalcm");
		if (hit >= 0) return hit;
		hit = fileHit("squad-template", "squadtmpl");
		if (hit >= 0) return hit;
		hit = fileHit("stability-cleanse", "stabcleanse");
		if (hit >= 0) return hit;
		hit = fileHit("material-conversions", "matconv");
		if (hit >= 0) return hit;
		hit = fileHit("legendary-paths", "legpaths");
		if (hit >= 0) return hit;
		hit = fileHit("mount-unlock", "mounts");
		if (hit >= 0) return hit;
		hit = fileHit("daily-weekly", "dailyweekly");
		if (hit >= 0) return hit;
		hit = fileHit("live-news", "live_news");
		if (hit >= 0) return hit;
		hit = fileHit("live-fashion", "live_fashion");
		if (hit >= 0) return hit;
		hit = fileHit("live-tp", "live_tp");
		if (hit >= 0) return hit;
		/* live-progress.html → Account → Progress (no Browse site id). */
		hit = fileHit("currency-sinks", "currencysinks");
		if (hit >= 0) return hit;
		hit = fileHit("ascended-start", "ascendedstart");
		if (hit >= 0) return hit;
		hit = fileHit("portals-pulls", "portalspulls");
		if (hit >= 0) return hit;
		hit = fileHit("homestead-extras", "homestead");
		if (hit >= 0) return hit;
		hit = fileHit("wvw-consumables", "wvwcons");
		if (hit >= 0) return hit;
		return -1;
	}

	EnsureUrlKeys();
	if (!gUrlKeysReady)
		return -1;

	const std::string live = UrlHostPath(url, false);
	const std::string liveHost = UrlHostPath(url, true);
	if (live.empty() || liveHost.empty())
		return -1;

	const auto hostIt = gUrlKeysByHost.find(liveHost);
	if (hostIt == gUrlKeysByHost.end())
		return -1;

	/* Candidates are longest-path-first — first path hit is the best match. */
	int hostFallback = -1;
	for (int i : hostIt->second)
	{
		const SiteUrlKey& key = gUrlKeys[i];
		if (!key.http || key.path.empty())
			continue;
		if (live == key.path || live.rfind(key.pathSlash, 0) == 0 || live.rfind(key.path, 0) == 0)
			return i;
		if (hostFallback < 0)
			hostFallback = i;
	}

	return hostFallback;
}
