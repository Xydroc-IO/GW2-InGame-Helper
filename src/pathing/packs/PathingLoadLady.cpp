#include "PathingTrails.h"

#include "Globals.h"
#include "PathingIndex.h"
#include "PathingParse.h"

#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace PathingDetail
{

	bool PrefixMatchesType(const std::string& typeLow, const std::string& prefixLow)
	{
		if (prefixLow.empty() || typeLow.empty())
			return false;
		if (typeLow == prefixLow)
			return true;
		return typeLow.size() > prefixLow.size() &&
			typeLow.compare(0, prefixLow.size(), prefixLow) == 0 &&
			typeLow[prefixLow.size()] == '.';
	}

	bool TypeHasLadyMountShortcut(const std::string& typeLow);
	bool IsLadyShortcutTypeLabel(const char* label);

	bool IsMountShortcutMarker(const PathingTrails::Marker& marker)
	{
		/* Type path first - iconId is empty until pack extract + Nexus upload. */
		if (IsLadyShortcutTypeLabel(marker.label))
			return true;
		if (!marker.iconId[0])
			return false;
		/* IconTextureId keeps alnum from path -> ...Images_Mounts_Mount_Raptor... */
		return std::strstr(marker.iconId, "Mounts") != nullptr ||
			std::strstr(marker.iconId, "mounts") != nullptr;
	}

	bool MarkerShownInWorld(const PathingTrails::Marker& marker)
	{
		if (IsMountShortcutMarker(marker))
			return true;
		return marker.inGameVisible;
	}

	bool MarkerShownOnCompass(const PathingTrails::Marker& marker)
	{
		if (IsMountShortcutMarker(marker))
			return true;
		return marker.minimapVisible;
	}

	/* Lady map-completion editions live at legs.map.<region>.<map>.<edition>...
	   Editions: barefoot | all/main/withmounts | wp. Do not treat "main"/"wp"
	   outside legs.map (festivals/chests) as a route edition. */
	bool LadyMapRouteEdition(const std::string& typeLow, std::string& outEdition)
	{
		if (typeLow.size() < 10)
			return false;
		if (typeLow.compare(0, 9, "legs.map.") != 0 &&
			typeLow.compare(0, 9, "leag.map.") != 0)
			return false;
		size_t start = 9; /* after legs.map. / leag.map. */
		for (int i = 0; i < 2; ++i)
		{
			const size_t dot = typeLow.find('.', start);
			if (dot == std::string::npos)
				return false;
			start = dot + 1;
		}
		const size_t dot = typeLow.find('.', start);
		outEdition = (dot == std::string::npos)
			? typeLow.substr(start)
			: typeLow.substr(start, dot - start);
		return !outEdition.empty();
	}

	bool IsLadyWithMountsEdition(const std::string& seg)
	{
		return seg == "all" || seg == "main" || seg == "withmounts";
	}

	bool IsLadyRouteEditionSeg(const std::string& seg)
	{
		return seg == "barefoot" || IsLadyWithMountsEdition(seg) || seg == "wp";
	}

	/* Mount shortcut category leaves (icons under Data/Images/Mounts/). */
	bool IsLadyMountShortcutSeg(const std::string& seg)
	{
		return seg == "mount" || seg == "mounted" || seg == "mounts" ||
			seg == "beetle" || seg == "griffon" || seg == "jackal" ||
			seg == "raptor" || seg == "skimmer" || seg == "skyscale" ||
			seg == "skyscal" || seg == "springer" || seg == "warclaw" ||
			seg == "turtle" || seg == "dismount" || seg == "leap" ||
			seg == "hover" || seg == "bof";
	}

	bool TypeHasLadyMountShortcut(const std::string& typeLow)
	{
		size_t start = 0;
		while (start <= typeLow.size())
		{
			const size_t dot = typeLow.find('.', start);
			const std::string seg = (dot == std::string::npos)
				? typeLow.substr(start)
				: typeLow.substr(start, dot - start);
			if (IsLadyMountShortcutSeg(seg))
				return true;
			if (dot == std::string::npos)
				break;
			start = dot + 1;
		}
		return false;
	}

	bool IsLadyBfsPath(const std::string& typeLow)
	{
		if (typeLow.find(".bfs.") != std::string::npos)
			return true;
		/* Shortcut trails typed as legs.map.<region>.bfs (no trailing leaf). */
		return typeLow.size() >= 4 &&
			typeLow.compare(typeLow.size() - 4, 4, ".bfs") == 0;
	}

	bool IsLadyShortcutTypeLabel(const char* label)
	{
		if (!label || !label[0])
			return false;
		const std::string typeLow = ToLower(label);
		if (IsLadyBfsPath(typeLow))
			return true;
		return TypeHasLadyMountShortcut(typeLow);
	}

	bool TypeCategoryEnabled(const std::string& type, const std::vector<std::string>& enabled)
	{
		if (type.empty() || enabled.empty())
			return false;
		const std::string typeLow = ToLower(type);
		for (const std::string& p : enabled)
		{
			if (PrefixMatchesType(typeLow, ToLower(p)))
				return true;
		}
		return false;
	}

	bool IsLadyHeartPath(const std::string& typeLow)
	{
		if (typeLow.find("heartpath") != std::string::npos)
			return true;
		if (typeLow.find(".heartinfo") != std::string::npos)
			return true;
		return typeLow.size() >= 10 &&
			typeLow.compare(typeLow.size() - 10, 10, ".heartinfo") == 0;
	}

	bool IsLadyHeroPointTrainPath(const std::string& typeLow)
	{
		/* legs.hp.* / leag.hp.* - not map-completion ...barefoot.hp markers. */
		return typeLow == "legs.hp" || typeLow == "leag.hp" ||
			(typeLow.size() > 8 && typeLow.compare(0, 8, "legs.hp.") == 0) ||
			(typeLow.size() > 8 && typeLow.compare(0, 8, "leag.hp.") == 0);
	}

	bool TypeEnabledWithEnabled(const std::string& type, const std::vector<std::string>& enabled)
	{
		if (type.empty() || enabled.empty())
			return false;
		const std::string typeLow = ToLower(type);

		/* Lady Elyssa Features - current map content only via loaded map trails. */
		const bool ladyPack =
			typeLow == "legs" || typeLow == "leag" ||
			(typeLow.size() > 5 && typeLow.compare(0, 5, "legs.") == 0) ||
			(typeLow.size() > 5 && typeLow.compare(0, 5, "leag.") == 0);
		if (ladyPack)
		{
			const bool bareOn = G::LadyBarefoot;
			const bool wpOn = G::LadyWpOnly;
			const bool mountsOn = G::LadyWithMounts;
			const bool heartsOn = G::LadyHearts;
			const bool hpTrainOn = G::LadyHeroPointTrain;
			const bool bfs = IsLadyBfsPath(typeLow);
			std::string mapEd;
			const bool onMapRoute = LadyMapRouteEdition(typeLow, mapEd);

			/* Hero Point Train / Categories -> Hero Points (legs.hp.*). */
			if (IsLadyHeroPointTrainPath(typeLow))
				return hpTrainOn && TypeCategoryEnabled(type, enabled);

			/* Heart trails/markers - own toggle (pulled out of Barefoot/Mounts). */
			if (IsLadyHeartPath(typeLow))
				return heartsOn && TypeCategoryEnabled(type, enabled);

			/* Barefoot Shortcuts: trails + markers under ...bfs... (Barefoot only). */
			if (bfs)
			{
				if (!bareOn)
					return false;
				return TypeCategoryEnabled(type, enabled);
			}

			if (onMapRoute)
			{
				/* Barefoot: foot trails + markers on this map. */
				if (mapEd == "barefoot")
				{
					if (!bareOn)
						return false;
					return TypeCategoryEnabled(type, enabled);
				}
				/* WP Only: WP trails + markers + shortcuts under ...map.<zone>.wp*. */
				if (mapEd == "wp")
				{
					if (!wpOn)
						return false;
					return TypeCategoryEnabled(type, enabled);
				}
				/* With Mounts: mount MC trails + mount-guide markers/shortcuts. */
				if (IsLadyWithMountsEdition(mapEd) ||
					(IsLadyMountShortcutSeg(mapEd) && !IsLadyRouteEditionSeg(mapEd)))
				{
					if (!mountsOn)
						return false;
					return TypeCategoryEnabled(type, enabled);
				}
				/* Other map-route leaves (lanterns, etc.) - Categories only. */
				return TypeCategoryEnabled(type, enabled);
			}
			/* Other Lady trees (bounty, fishing, mapt, ranger, rifts, mape, ...)
			   follow Categories - Features exclusivity is map-route / hearts / HP only. */
		}

		return TypeCategoryEnabled(type, enabled);
	}

	bool TypeEnabledLocked(const std::string& type)
	{
		return TypeEnabledWithEnabled(type, gEnabledPaths);
	}

	bool CategoryUiEnabledLocked(const std::string& path)
	{
		if (path.empty() || gEnabledPaths.empty())
			return false;
		const std::string low = ToLower(path);
		bool covered = false;
		for (const std::string& p : gEnabledPaths)
		{
			const std::string el = ToLower(p);
			/* Ancestor covers this node, or this node covers an enabled child. */
			if (PrefixMatchesType(low, el) || PrefixMatchesType(el, low) || low == el)
			{
				covered = true;
				break;
			}
		}
		if (!covered)
			return false;
		/* Hero Points / Hero Point Train share legs.hp - Features gate drives the checkbox
		   so Categories toggles stay meaningful under an enabled Lady root. */
		if (low == "legs.hp" || low == "leag.hp" ||
			(low.size() > 8 && low.compare(0, 8, "legs.hp.") == 0) ||
			(low.size() > 8 && low.compare(0, 8, "leag.hp.") == 0))
			return G::LadyHeroPointTrain;
		return true;
	}

	void InsertCatPath(std::vector<PathingTrails::Category>& roots, const std::string& type)
	{
		if (type.empty())
			return;
		std::vector<std::string> parts;
		size_t start = 0;
		while (start < type.size() && parts.size() < 16)
		{
			size_t dot = type.find('.', start);
			if (dot == std::string::npos)
			{
				parts.push_back(type.substr(start));
				break;
			}
			parts.push_back(type.substr(start, dot - start));
			start = dot + 1;
		}
		if (parts.empty())
			return;

		std::vector<PathingTrails::Category>* level = &roots;
		std::string path;
		for (size_t i = 0; i < parts.size(); ++i)
		{
			if (!path.empty())
				path += '.';
			path += parts[i];
			PathingTrails::Category* found = nullptr;
			for (PathingTrails::Category& c : *level)
			{
				if (c.path == path)
				{
					found = &c;
					break;
				}
			}
			if (!found)
			{
				PathingTrails::Category neu;
				neu.path = path;
				neu.label = parts[i];
				neu.trails = 0;
				neu.enabled = false;
				level->push_back(std::move(neu));
				found = &level->back();
			}
			++found->trails;
			level = &found->children;
		}
	}

	void MarkEnabled(std::vector<PathingTrails::Category>& nodes)
	{
		for (PathingTrails::Category& c : nodes)
		{
			c.enabled = CategoryUiEnabledLocked(c.path);
			MarkEnabled(c.children);
		}
	}


} // namespace PathingDetail
