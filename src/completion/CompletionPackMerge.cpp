#include "CompletionInternal.h"

#include "Globals.h"
#include "PathingTrails.h"

#include <cmath>
#include <vector>

namespace CompletionDetail
{
	namespace
	{
		constexpr float kDedupeContinent = 100.f;

		bool NearExisting(ObjKind kind, uint32_t mapId, float cx, float cy)
		{
			const size_t n = ObjectiveCount();
			const float lim2 = kDedupeContinent * kDedupeContinent;
			for (size_t i = 0; i < n; ++i)
			{
				Objective* o = ObjectiveAt(i);
				if (!o || o->kind != kind || o->mapId != mapId || !o->hasCoord)
					continue;
				const float dx = o->continentX - cx;
				const float dy = o->continentY - cy;
				if (dx * dx + dy * dy <= lim2)
					return true;
			}
			return false;
		}

		ObjKind KindFromPack(PackMarkerKind pk)
		{
			switch (pk)
			{
			case PackMarkerKind::Heart: return ObjKind::Heart;
			case PackMarkerKind::Hero: return ObjKind::Hero;
			case PackMarkerKind::Achievement: return ObjKind::Achievement;
			default: return ObjKind::Poi;
			}
		}

		void MergeOne(const PathingTrails::IndexedMarkerSnapshot& m, PackMarkerKind pk,
			bool dedupeNear, uint32_t fetchMapId)
		{
			float cx = 0.f, cy = 0.f;
			bool haveCoord = PathingTrails::TryWorldToContinentCached(m.mapId, m.wx, m.wz, &cx, &cy);
			if (!haveCoord && fetchMapId != 0 && m.mapId == fetchMapId)
				haveCoord = PathingTrails::WorldToContinentForMap(m.mapId, m.wx, m.wz, &cx, &cy);
			const ObjKind kind = KindFromPack(pk);
			if (dedupeNear && haveCoord && NearExisting(kind, m.mapId, cx, cy))
				return;

			char name[96]{};
			FormatPackMarkerName(m.tipName, m.type, m.mapId, name, sizeof(name));
			UpsertMapEntry(m.mapId, nullptr, nullptr);
			UpsertObjective(
				StablePackObjectiveId(m.guid, m.type, m.mapId, m.wx, m.wz),
				m.mapId, kind, name, cx, cy, haveCoord, m.type);
		}
	}

	void MergePackMarkers()
	{
		if (PathingTrails::IndexedMarkerCount() == 0)
			return;
		std::vector<PathingTrails::IndexedMarkerSnapshot> markers;
		PathingTrails::CopyIndexedMarkers(markers);
		if (markers.empty())
			return;

		uint32_t fetchMap = gFocusMapId;
		if (fetchMap == 0 && G::Mumble && G::Mumble->context_len >= sizeof(MumbleContext))
		{
			const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
			if (ctx) fetchMap = ctx->mapId;
		}

		/* Pass 0: Lady Heart/HP. Pass 1: Tekkit Heart/HP (dedupe). Pass 2: AP. */
		for (int pass = 0; pass < 3; ++pass)
		{
			for (const PathingTrails::IndexedMarkerSnapshot& m : markers)
			{
				const PackMarkerKind pk = ClassifyPackMarker(m.type, m.packLeaf);
				if (pk == PackMarkerKind::None)
					continue;
				const bool ladyMc = IsLadyPreferredPackLeaf(m.packLeaf);
				if (pass == 0)
				{
					if (pk == PackMarkerKind::Achievement || !ladyMc)
						continue;
					MergeOne(m, pk, false, fetchMap);
				}
				else if (pass == 1)
				{
					if (pk == PackMarkerKind::Achievement || ladyMc)
						continue;
					MergeOne(m, pk, true, fetchMap);
				}
				else if (pk == PackMarkerKind::Achievement)
				{
					MergeOne(m, pk, false, fetchMap);
				}
			}
		}
	}
}
