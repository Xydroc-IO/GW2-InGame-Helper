#pragma once

#include "CompletionShared.h"

namespace CompletionDetail
{
	constexpr float kPadW = 440.f;
	constexpr float kPadH = 480.f;

	/* Catalog writes used by floors merge + pack merge (domain-internal). */
	void UpsertMapEntry(uint32_t id, const char* name, const char* region);
	void UpsertObjective(uint32_t id, uint32_t mapId, ObjKind kind, const char* name,
		float cx, float cy, bool hasCoord, const char* packType);
	void MergePackMarkers();

	/* Floors API merge (CompletionFloors.cpp). */
	void ResetFloorMergeState();
	void SeedCatalogMaps();
	void EnrichMapNamesFromFloors();
	bool FloorPoisMerged(uint32_t mapId);
	void MergeFloorPois(uint32_t mapId);
	void MergeFloorPoisBackground();

	/* Pack marker classify / stable ids (CompletionPackClassify.cpp). */
	enum class PackMarkerKind : int
	{
		None = 0,
		Heart,
		Hero,
		Achievement
	};
	PackMarkerKind ClassifyPackMarker(const char* type, const char* packLeaf);
	bool IsLadyPreferredPackLeaf(const char* packLeaf);
	uint32_t StablePackObjectiveId(const char* guid, const char* type, uint32_t mapId,
		float wx, float wz);
	void FormatPackMarkerName(const char* tipName, const char* type, uint32_t mapId,
		char* out, size_t outLen);
}
