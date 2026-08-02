#pragma once

#include <string>
#include <vector>

/* Official API waypoint / POI index — name search, by map, this-map (Mumble). */
namespace WaypointsData
{
	struct Poi
	{
		int mapId = 0;
		int id = 0;
		std::string mapName;
		std::string name;
		std::string type; /* waypoint | landmark | vista | unlock */
		std::string chatLink;
	};

	struct MapRow
	{
		int id = 0;
		std::string name;
		int waypointCount = 0;
	};

	void EnsureLoaded(bool force = false);
	void Tick();
	bool Busy();
	bool Ready();
	const char* Status();

	/* Case-insensitive substring match. waypointsOnly skips landmarks/vistas. */
	void Search(const char* query, bool waypointsOnly, std::vector<Poi>& out, size_t maxN = 80);

	void ListForMap(int mapId, bool waypointsOnly, std::vector<Poi>& out);

	void ListMaps(const char* filter, std::vector<MapRow>& out, size_t maxN = 80);

	/* MumbleLink map id, or 0 if unavailable. */
	int CurrentMapId();
}
