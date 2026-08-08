#include "CompletionInternal.h"

#include "WaypointsData.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace CompletionDetail
{
	namespace
	{
		std::unordered_set<uint32_t> gFloorMergedMaps;
		bool gMapNamesEnriched = false;
		constexpr int kBgMergePerTick = 8;

		ObjKind KindFromApiType(const std::string& type)
		{
			if (type == "waypoint") return ObjKind::Waypoint;
			if (type == "vista") return ObjKind::Vista;
			if (type == "unlock") return ObjKind::Mastery;
			return ObjKind::Poi;
		}
	}

	void ResetFloorMergeState()
	{
		gFloorMergedMaps.clear();
		gMapNamesEnriched = false;
	}

	void SeedCatalogMaps()
	{
		UpsertMapEntry(15, "Queensdale", "Kryta");
		UpsertMapEntry(34, "Caledon Forest", "Maguuma Jungle");
		UpsertMapEntry(50, "Lion's Arch", "Kryta");
		UpsertMapEntry(28, "Wayfarer Foothills", "Shiverpeak Mountains");
		UpsertMapEntry(1015, "The Silverwastes", "Maguuma Wastes");
		VisitHierarchy([](uint32_t mapId, const char*, const char* region,
			const char* name, void*) {
				UpsertMapEntry(mapId, name, region);
			}, nullptr);
	}

	void EnrichMapNamesFromFloors()
	{
		if (gMapNamesEnriched)
			return;
		std::vector<WaypointsData::MapRow> maps;
		WaypointsData::ListMaps(nullptr, maps, 4000);
		for (const auto& mr : maps)
		{
			if (mr.id <= 0 || mr.name.empty())
				continue;
			UpsertMapEntry(static_cast<uint32_t>(mr.id), mr.name.c_str(), nullptr);
		}
		gMapNamesEnriched = true;
	}

	bool FloorPoisMerged(uint32_t mapId)
	{
		return mapId != 0 && gFloorMergedMaps.count(mapId) != 0;
	}

	void MergeFloorPois(uint32_t mapId)
	{
		if (mapId == 0)
			return;
		WaypointsData::EnsureLoaded(false);
		WaypointsData::Tick();
		if (!WaypointsData::Ready())
			return;
		std::vector<WaypointsData::Poi> pois;
		WaypointsData::ListForMap(static_cast<int>(mapId), false, pois);
		for (const auto& p : pois)
		{
			if (p.id <= 0)
				continue;
			UpsertObjective(static_cast<uint32_t>(p.id), static_cast<uint32_t>(p.mapId),
				KindFromApiType(p.type), p.name.c_str(),
				p.continentX, p.continentY, p.hasCoord, nullptr);
		}
		gFloorMergedMaps.insert(mapId);
	}

	void MergeFloorPoisBackground()
	{
		struct Pending
		{
			std::vector<uint32_t>* ids;
			const std::unordered_set<uint32_t>* done;
		};
		std::vector<uint32_t> pending;
		Pending ctx{ &pending, &gFloorMergedMaps };
		VisitHierarchy([](uint32_t mapId, const char*, const char*, const char*, void* v) {
				auto* p = static_cast<Pending*>(v);
				if (p->done->count(mapId) == 0)
					p->ids->push_back(mapId);
			}, &ctx);
		int n = 0;
		for (uint32_t id : pending)
		{
			MergeFloorPois(id);
			if (++n >= kBgMergePerTick)
				break;
		}
	}
}
