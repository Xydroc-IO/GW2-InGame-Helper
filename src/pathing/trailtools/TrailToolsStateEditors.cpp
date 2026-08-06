#include "TrailToolsShared.h"
#include "TrailToolsBinds.h"

#include "Globals.h"

#include <cstdio>

namespace TrailToolsDetail
{
	namespace
	{
		struct TrailActiveBackup
		{
			DraftTrail trail{};
			char       stem[64]{};
			bool       dirty = false;
			int        selectedPoint = -1;
			bool       valid = false;
		};
		TrailActiveBackup gTrailActiveBackup{};
	}

	bool AnyAuthoringPadOpen()
	{
		if (G::ShowTrailTools || gShowTrailsDesk || gShowMarkersDesk)
			return true;
		for (int i = 0; i < kMaxTrailEditors; ++i)
		{
			if (gTrailEditors[i].open)
				return true;
		}
		for (int i = 0; i < kMaxMarkerEditors; ++i)
		{
			if (gMarkerEditors[i].open)
				return true;
		}
		return false;
	}

	void CloseAllPopouts()
	{
		gShowTrailsDesk = false;
		gShowMarkersDesk = false;
		gPopoutTrails = false;
		gPopoutMarkers = false;
		gTrailRecordSlot = -1;
		for (int i = 0; i < kMaxTrailEditors; ++i)
			gTrailEditors[i].open = false;
		for (int i = 0; i < kMaxMarkerEditors; ++i)
			gMarkerEditors[i].open = false;
	}

	void OpenTrailsDesk()
	{
		if (!gShowTrailsDesk)
			gPlaceOnceTrailsDesk = true;
		gShowTrailsDesk = true;
		gFocusTrailsDesk = true;
		gPopoutTrails = true; /* legacy unload / AnyOpen mirrors */
	}

	void OpenMarkersDesk()
	{
		if (!gShowMarkersDesk)
			gPlaceOnceMarkersDesk = true;
		gShowMarkersDesk = true;
		gFocusMarkersDesk = true;
		gPopoutMarkers = true;
	}

	void PushTrailEditorToActive(int slot)
	{
		if (slot < 0 || slot >= kMaxTrailEditors || !gTrailEditors[slot].open)
			return;
		if (gTrailActiveBackup.valid)
			return;
		TrailEditorSlot& s = gTrailEditors[slot];
		gTrailActiveBackup.trail = gDraft.active;
		std::snprintf(gTrailActiveBackup.stem, sizeof(gTrailActiveBackup.stem), "%s",
			gDraft.trailFileStem);
		gTrailActiveBackup.dirty = gDraft.trailDirty;
		gTrailActiveBackup.selectedPoint = gDraft.selectedPoint;
		gTrailActiveBackup.valid = true;
		gDraft.active = s.trail;
		std::snprintf(gDraft.trailFileStem, sizeof(gDraft.trailFileStem), "%s", s.stem);
		gDraft.trailDirty = s.dirty;
		gDraft.selectedPoint = s.selectedPoint;
	}

	void PopTrailEditorFromActive(int slot)
	{
		if (!gTrailActiveBackup.valid)
			return;
		if (slot >= 0 && slot < kMaxTrailEditors && gTrailEditors[slot].open)
		{
			TrailEditorSlot& s = gTrailEditors[slot];
			s.trail = gDraft.active;
			std::snprintf(s.stem, sizeof(s.stem), "%s", gDraft.trailFileStem);
			s.dirty = gDraft.trailDirty;
			s.selectedPoint = gDraft.selectedPoint;
		}
		gDraft.active = gTrailActiveBackup.trail;
		std::snprintf(gDraft.trailFileStem, sizeof(gDraft.trailFileStem), "%s",
			gTrailActiveBackup.stem);
		gDraft.trailDirty = gTrailActiveBackup.dirty;
		gDraft.selectedPoint = gTrailActiveBackup.selectedPoint;
		gTrailActiveBackup = {};
	}

	static void InitTrailEditorSlot(TrailEditorSlot& s, int index)
	{
		s.open = true;
		s.placeOnce = true;
		s.focus = true;
		s.dirty = false;
		s.selectedPoint = -1;
		s.geomX = -1.f;
		s.geomY = -1.f;
		s.geomW = 0.f;
		s.geomH = 0.f;
		if (index == 0)
			std::snprintf(s.stem, sizeof(s.stem), "Trail");
		else
			std::snprintf(s.stem, sizeof(s.stem), "Trail%d", index + 1);
		s.trail = {};
		s.trail.type = gDraft.trailType[0] ? gDraft.trailType
			: (RootCategoryName() + ".t.extrail");
		s.trail.fileRel = std::string("Data/") + gDraft.packName + "/Trails/" +
			s.stem + ".trl";
	}

	int OpenTrailEditorSlot(int slot)
	{
		if (slot < 0 || slot >= kMaxTrailEditors)
			return -1;
		TrailEditorSlot& s = gTrailEditors[slot];
		if (s.open)
		{
			s.focus = true;
			gTrailRecordSlot = slot;
			SetStatus("Focused Trails%d.", slot + 1);
			return slot;
		}
		InitTrailEditorSlot(s, slot);
		/* Only seed slot 0 from hub active when first opening it. */
		if (slot == 0 && gDraft.active.points.size() >= 2)
		{
			s.trail = gDraft.active;
			if (gDraft.trailFileStem[0])
				std::snprintf(s.stem, sizeof(s.stem), "%s", gDraft.trailFileStem);
			s.dirty = gDraft.trailDirty;
		}
		gTrailRecordSlot = slot;
		SetStatus("Opened Trails%d (others stay open).", slot + 1);
		return slot;
	}

	int OpenNewTrailEditor()
	{
		for (int i = 0; i < kMaxTrailEditors; ++i)
		{
			if (gTrailEditors[i].open)
				continue;
			return OpenTrailEditorSlot(i);
		}
		SetStatus("Already have %d trail windows open — close one first.", kMaxTrailEditors);
		return -1;
	}

	int OpenMarkerEditor(int poiIndex, bool forceNew)
	{
		if (poiIndex < 0 || poiIndex >= static_cast<int>(gDraft.pois.size()))
		{
			SetStatus("Select a marker on the desk first.");
			return -1;
		}
		if (!forceNew)
		{
			for (int i = 0; i < kMaxMarkerEditors; ++i)
			{
				if (gMarkerEditors[i].open && gMarkerEditors[i].poiIndex == poiIndex)
				{
					gMarkerEditors[i].focus = true;
					SetStatus("Focused Markers%d (POI %d).", i + 1, poiIndex);
					return i;
				}
			}
		}
		for (int i = 0; i < kMaxMarkerEditors; ++i)
		{
			if (gMarkerEditors[i].open)
				continue;
			MarkerEditorSlot& s = gMarkerEditors[i];
			s = {};
			s.open = true;
			s.placeOnce = true;
			s.focus = true;
			s.poiIndex = poiIndex;
			gDraft.selectedPoi = poiIndex;
			SetStatus("Opened Markers%d for POI %d (others stay open).", i + 1, poiIndex);
			return i;
		}
		SetStatus("Already have %d marker windows open — close one first.", kMaxMarkerEditors);
		return -1;
	}

	int OpenNewMarkerEditor()
	{
		int poi = gDraft.selectedPoi;
		if (poi < 0 || poi >= static_cast<int>(gDraft.pois.size()))
		{
			/* Drop one so there is something to edit. */
			TrailToolsBinds::ActionPlaceMarker(-1);
			poi = gDraft.selectedPoi;
		}
		if (poi < 0 || poi >= static_cast<int>(gDraft.pois.size()))
		{
			SetStatus("Could not place a marker — check Mumble pose / Keybinds.");
			return -1;
		}
		return OpenMarkerEditor(poi, true);
	}
}
