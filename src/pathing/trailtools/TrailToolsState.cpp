#include "TrailToolsShared.h"
#include "TrailToolsBinds.h"

#include "AddonPaths.h"
#include "Globals.h"

#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <wincrypt.h>

#include "TrailToolsLooks.inc"

namespace TrailToolsDetail
{
	DraftPack gDraft{};
	bool      gPlaceOnce = false;
	bool      gFocus = false;
	int       gTab = 0;

	TrailEditorSlot  gTrailEditors[kMaxTrailEditors]{};
	MarkerEditorSlot gMarkerEditors[kMaxMarkerEditors]{};
	bool  gShowTrailsDesk = false;
	bool  gShowMarkersDesk = false;
	bool  gPlaceOnceTrailsDesk = false;
	bool  gFocusTrailsDesk = false;
	bool  gPlaceOnceMarkersDesk = false;
	bool  gFocusMarkersDesk = false;
	float gTrailsDeskX = -1.f, gTrailsDeskY = -1.f, gTrailsDeskW = 0.f, gTrailsDeskH = 0.f;
	float gMarkersDeskX = -1.f, gMarkersDeskY = -1.f, gMarkersDeskW = 0.f, gMarkersDeskH = 0.f;
	int   gTrailRecordSlot = -1;
	bool  gPopoutTrails = false;
	bool  gPopoutMarkers = false;

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

	void SetStatus(const char* fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		std::vsnprintf(gDraft.status, sizeof(gDraft.status), fmt, ap);
		va_end(ap);
	}

	void SanitizePackName(char* name, size_t len)
	{
		if (!name || len == 0)
			return;
		size_t w = 0;
		for (size_t i = 0; name[i] && w + 1 < len; ++i)
		{
			const unsigned char c = static_cast<unsigned char>(name[i]);
			if (std::isalnum(c) || c == '_' || c == '-')
				name[w++] = static_cast<char>(c);
		}
		name[w] = 0;
		if (!name[0])
			std::snprintf(name, len, "ExamplePack");
	}

	std::string RootCategoryName()
	{
		char buf[64]{};
		std::snprintf(buf, sizeof(buf), "%s", gDraft.packName);
		for (char* p = buf; *p; ++p)
			*p = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
		return buf;
	}

	void SeedDefaultCategories()
	{
		const std::string rootName = RootCategoryName();
		gDraft.root = {};
		gDraft.root.name = rootName;
		gDraft.root.displayName = gDraft.displayName[0] ? gDraft.displayName : gDraft.packName;

		CategoryNode markers;
		markers.name = "m";
		markers.displayName = "Markers";
		markers.fadeFar = 3500.f;
		CategoryNode exm;
		exm.name = "exm";
		exm.displayName = "Example Marker";
		exm.iconFile = std::string("Data/") + gDraft.packName + "/Markers/Marker_Disc.png";
		exm.iconSize = 1.f;
		exm.color = 0xFFFFC828u;
		markers.children.push_back(exm);
		gDraft.root.children.push_back(markers);

		CategoryNode trails;
		trails.name = "t";
		trails.displayName = "Trails";
		CategoryNode extrail;
		extrail.name = "extrail";
		extrail.displayName = "Example Trail";
		extrail.texture = std::string("Data/") + gDraft.packName + "/Markers/Trail_Chevron.png";
		extrail.fadeNear = 3000.f;
		extrail.fadeFar = 3500.f;
		extrail.trailScale = 1.f;
		extrail.color = 0xFFFFFFFFu;
		trails.children.push_back(extrail);
		gDraft.root.children.push_back(trails);

		std::snprintf(gDraft.markerType, sizeof(gDraft.markerType), "%s.m.exm", rootName.c_str());
		std::snprintf(gDraft.trailType, sizeof(gDraft.trailType), "%s.t.extrail", rootName.c_str());
		gDraft.active.type = gDraft.trailType;
		gDraft.active.fileRel = std::string("Data/") + gDraft.packName + "/Trails/Trail.trl";
	}

	std::string CategoryPath(const CategoryNode& node, const std::string& parentPath)
	{
		if (parentPath.empty())
			return node.name;
		return parentPath + "." + node.name;
	}

	void CollectLeafPaths(const CategoryNode& node, const std::string& parentPath,
		std::vector<std::string>& out, bool trailLeaves)
	{
		const std::string path = CategoryPath(node, parentPath);
		const bool isLeaf = node.children.empty();
		if (isLeaf)
		{
			const bool hasIcon = !node.iconFile.empty();
			const bool hasTex = !node.texture.empty();
			const bool underT = parentPath.find(".t") != std::string::npos ||
				node.name.find("trail") != std::string::npos ||
				node.name.find("extrail") != std::string::npos;
			const bool underM = parentPath.find(".m") != std::string::npos ||
				node.name.find("exm") != std::string::npos;
			bool looksTrail = false;
			bool looksMarker = false;
			if (underT && !underM)
				looksTrail = true;
			else if (underM && !underT)
				looksMarker = true;
			else if (hasTex && !hasIcon)
				looksTrail = true;
			else if (hasIcon && !hasTex)
				looksMarker = true;
			else if (hasTex && hasIcon)
			{
				/* Both assets: exclusive - trail list vs marker list, never both. */
				looksTrail = trailLeaves;
				looksMarker = !trailLeaves;
			}
			else
				looksMarker = !trailLeaves; /* bare leaf -> marker picker */
			if (trailLeaves ? looksTrail : looksMarker)
				out.push_back(path);
			return;
		}
		for (const CategoryNode& ch : node.children)
			CollectLeafPaths(ch, path, out, trailLeaves);
	}

	void RemapPrefix(std::string& s, const std::string& oldP, const std::string& newP)
	{
		if (oldP.empty() || oldP == newP || s.empty())
			return;
		if (s == oldP)
		{
			s = newP;
			return;
		}
		if (s.size() > oldP.size() && s.compare(0, oldP.size(), oldP) == 0 &&
			s[oldP.size()] == '.')
			s = newP + s.substr(oldP.size());
	}

	void RemapDataFolder(std::string& s, const std::string& oldPack, const std::string& newPack)
	{
		if (oldPack.empty() || oldPack == newPack || s.empty())
			return;
		const std::string a = std::string("Data/") + oldPack + "/";
		const std::string b = std::string("Data/") + newPack + "/";
		if (s.size() >= a.size() && s.compare(0, a.size(), a) == 0)
			s = b + s.substr(a.size());
	}

	void RemapCategoryNode(CategoryNode& n, const std::string& oldPack, const std::string& newPack)
	{
		RemapDataFolder(n.iconFile, oldPack, newPack);
		RemapDataFolder(n.texture, oldPack, newPack);
		for (auto& ch : n.children)
			RemapCategoryNode(ch, oldPack, newPack);
	}

	void RemapDraftAfterPackRename(const std::string& oldPackName, const std::string& oldRoot)
	{
		const std::string newPack = gDraft.packName;
		const std::string newRoot = RootCategoryName();
		if (oldRoot != newRoot)
		{
			RemapPrefix(gDraft.root.name, oldRoot, newRoot);
			gDraft.root.name = newRoot;
			auto remapChar = [&](char* buf, size_t len) {
				std::string s = buf;
				RemapPrefix(s, oldRoot, newRoot);
				std::snprintf(buf, len, "%s", s.c_str());
			};
			remapChar(gDraft.markerType, sizeof(gDraft.markerType));
			remapChar(gDraft.trailType, sizeof(gDraft.trailType));
			for (auto& p : gDraft.pois)
				RemapPrefix(p.type, oldRoot, newRoot);
			for (auto& t : gDraft.trails)
				RemapPrefix(t.type, oldRoot, newRoot);
			RemapPrefix(gDraft.active.type, oldRoot, newRoot);
		}
		if (oldPackName != newPack)
		{
			RemapCategoryNode(gDraft.root, oldPackName, newPack);
			for (auto& p : gDraft.pois)
				RemapDataFolder(p.iconFile, oldPackName, newPack);
			for (auto& t : gDraft.trails)
				RemapDataFolder(t.fileRel, oldPackName, newPack);
			RemapDataFolder(gDraft.active.fileRel, oldPackName, newPack);
		}
	}

	std::wstring AuthoringRoot()
	{
		return AddonPaths::EnsureUnder(AddonPaths::DataDir(), L"pathing\\authoring");
	}

	std::wstring PackDir()
	{
		SanitizePackName(gDraft.packName, sizeof(gDraft.packName));
		std::wstring rel = L"pathing\\authoring\\";
		for (const char* p = gDraft.packName; *p; ++p)
			rel.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
		return AddonPaths::EnsureUnder(AddonPaths::DataDir(), rel.c_str());
	}

	bool EnsureWorkspace()
	{
		const std::wstring pack = PackDir();
		if (pack.empty())
			return false;
		AddonPaths::EnsureUnder(pack, L"Data");
		{
			std::wstring dataRel = L"Data\\";
			for (const char* p = gDraft.packName; *p; ++p)
				dataRel.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
			dataRel += L"\\Markers";
			AddonPaths::EnsureUnder(pack, dataRel.c_str());
			dataRel = L"Data\\";
			for (const char* p = gDraft.packName; *p; ++p)
				dataRel.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
			dataRel += L"\\Trails";
			AddonPaths::EnsureUnder(pack, dataRel.c_str());
		}
		WriteDefaultAssets();
		return true;
	}

	bool WriteBytesW(const std::wstring& path, const unsigned char* data, size_t len)
	{
		if (path.empty() || !data || len == 0)
			return false;
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, data, static_cast<DWORD>(len), &written, nullptr);
		CloseHandle(h);
		return ok && written == len;
	}

	bool WriteDefaultAssets()
	{
		size_t n = 0;
		const TrailToolsLooks::EmbeddedPng* all = TrailToolsLooks::All(&n);

		std::wstring base = PackDir();
		base.push_back(L'\\');
		base += L"Data\\";
		for (const char* p = gDraft.packName; *p; ++p)
			base.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
		base += L"\\Markers\\";

		bool ok = true;
		for (size_t i = 0; i < n; ++i)
		{
			std::wstring path = base;
			for (const char* c = all[i].file; *c; ++c)
				path.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*c)));
			if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
				ok = WriteBytesW(path, all[i].data, all[i].len) && ok;
		}
		/* Compat aliases used by older drafts. */
		const std::wstring disc = base + L"Marker_Disc.png";
		const std::wstring chev = base + L"Trail_Chevron.png";
		const std::wstring aliasM = base + L"ExampleMarker.png";
		const std::wstring aliasT = base + L"Trail.png";
		if (GetFileAttributesW(aliasM.c_str()) == INVALID_FILE_ATTRIBUTES &&
			GetFileAttributesW(disc.c_str()) != INVALID_FILE_ATTRIBUTES)
			CopyFileW(disc.c_str(), aliasM.c_str(), FALSE);
		if (GetFileAttributesW(aliasT.c_str()) == INVALID_FILE_ATTRIBUTES &&
			GetFileAttributesW(chev.c_str()) != INVALID_FILE_ATTRIBUTES)
			CopyFileW(chev.c_str(), aliasT.c_str(), FALSE);
		return ok;
	}

	CategoryNode* FindCategoryByPath(CategoryNode& node, const std::string& wantPath,
		const std::string& parentPath)
	{
		const std::string path = CategoryPath(node, parentPath);
		if (path == wantPath)
			return &node;
		for (CategoryNode& ch : node.children)
		{
			if (CategoryNode* hit = FindCategoryByPath(ch, wantPath, path))
				return hit;
		}
		return nullptr;
	}

	const char* const* TrailLookPresetNames(int* count)
	{
		static const char* kNames[] = {
			"Blish Chevron", "Cyan Ribbon", "Dashed", "Heart Yellow", "Custom path"
		};
		if (count) *count = 5;
		return kNames;
	}

	const char* const* MarkerLookPresetNames(int* count)
	{
		static const char* kNames[] = {
			"Gold Disc", "Red Pin", "Star", "Square", "Custom path"
		};
		if (count) *count = 5;
		return kNames;
	}

	void ApplyTrailLookPreset(int presetIndex)
	{
		EnsureWorkspace();
		const std::string want = gDraft.trailType[0]
			? std::string(gDraft.trailType) : (RootCategoryName() + ".t.extrail");
		CategoryNode* leaf = FindCategoryByPath(gDraft.root, want);
		if (!leaf)
		{
			SetStatus("No trail category for type %s.", want.c_str());
			return;
		}
		const std::string prefix = std::string("Data/") + gDraft.packName + "/Markers/";
		switch (presetIndex)
		{
		case 0:
			leaf->texture = prefix + "Trail_Chevron.png";
			leaf->color = 0xFFFFFFFFu;
			leaf->trailScale = 1.f;
			leaf->fadeNear = 3000.f;
			leaf->fadeFar = 3500.f;
			break;
		case 1:
			leaf->texture = prefix + "Trail_Ribbon.png";
			leaf->color = 0xFF50DCFF;
			leaf->trailScale = 1.15f;
			leaf->fadeNear = 2800.f;
			leaf->fadeFar = 3400.f;
			break;
		case 2:
			leaf->texture = prefix + "Trail_Dashed.png";
			leaf->color = 0xFFFFFFFFu;
			leaf->trailScale = 1.f;
			leaf->fadeNear = 3000.f;
			leaf->fadeFar = 3500.f;
			break;
		case 3:
			leaf->texture = prefix + "Trail_Heart.png";
			leaf->color = 0xFFFFD228u;
			leaf->trailScale = 1.35f;
			leaf->fadeNear = 2500.f;
			leaf->fadeFar = 3200.f;
			break;
		default:
			SetStatus("Custom - edit texture path on the category below.");
			return;
		}
		leaf->iconFile.clear();
		SetStatus("Trail look -> %s", TrailLookPresetNames(nullptr)[presetIndex]);
	}

	void ApplyMarkerLookPreset(int presetIndex)
	{
		EnsureWorkspace();
		const std::string want = gDraft.markerType[0]
			? std::string(gDraft.markerType) : (RootCategoryName() + ".m.exm");
		CategoryNode* leaf = FindCategoryByPath(gDraft.root, want);
		if (!leaf)
		{
			SetStatus("No marker category for type %s.", want.c_str());
			return;
		}
		const std::string prefix = std::string("Data/") + gDraft.packName + "/Markers/";
		switch (presetIndex)
		{
		case 0:
			leaf->iconFile = prefix + "Marker_Disc.png";
			leaf->color = 0xFFFFC828u;
			leaf->iconSize = 1.f;
			break;
		case 1:
			leaf->iconFile = prefix + "Marker_Pin.png";
			leaf->color = 0xFFFF5050u;
			leaf->iconSize = 1.1f;
			break;
		case 2:
			leaf->iconFile = prefix + "Marker_Star.png";
			leaf->color = 0xFFFFE650u;
			leaf->iconSize = 1.15f;
			break;
		case 3:
			leaf->iconFile = prefix + "Marker_Square.png";
			leaf->color = 0xFF78C8FFu;
			leaf->iconSize = 1.f;
			break;
		default:
			SetStatus("Custom - edit iconFile on the category below.");
			return;
		}
		leaf->texture.clear();
		leaf->fadeNear = -1.f;
		leaf->fadeFar = 3500.f;
		SetStatus("Marker look -> %s", MarkerLookPresetNames(nullptr)[presetIndex]);
	}

	bool HasDraftPreview()
	{
		if (!gDraft.previewEnabled)
			return false;
		if (gDraft.active.points.size() >= 2 && gDraft.active.mapId != 0)
			return true;
		uint32_t mapId = 0;
		float x = 0.f, y = 0.f, z = 0.f;
		if (!ReadMumblePose(mapId, x, y, z))
			return !gDraft.pois.empty();
		for (const DraftPoi& p : gDraft.pois)
		{
			if (p.mapId == mapId)
				return true;
		}
		return false;
	}

	bool OpenAuthoringFolder()
	{
		EnsureWorkspace();
		const std::wstring dir = PackDir();
		if (dir.empty())
			return false;
		return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", dir.c_str(),
			nullptr, nullptr, SW_SHOWNORMAL)) > 32;
	}

	void CopyClipboard(const char* text)
	{
		if (!text || !OpenClipboard(nullptr))
			return;
		EmptyClipboard();
		const size_t n = std::strlen(text) + 1;
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, n);
		if (mem)
		{
			void* p = GlobalLock(mem);
			if (p)
			{
				std::memcpy(p, text, n);
				GlobalUnlock(mem);
				SetClipboardData(CF_TEXT, mem);
			}
			else
				GlobalFree(mem);
		}
		CloseClipboard();
	}

	bool ReadMumblePose(uint32_t& mapId, float& x, float& y, float& z)
	{
		mapId = 0;
		x = y = z = 0.f;
		if (!G::Mumble || G::Mumble->uiTick == 0)
			return false;
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		if (!ctx || ctx->mapId == 0)
			return false;
		mapId = ctx->mapId;
		x = G::Mumble->fAvatarPosition[0];
		y = G::Mumble->fAvatarPosition[1];
		z = G::Mumble->fAvatarPosition[2];
		return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
	}

	std::string MakeGuidBase64()
	{
		static bool sCom;
		if (!sCom)
		{
			CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			sCom = true;
		}
		GUID g{};
		if (FAILED(CoCreateGuid(&g)))
			return "AAAAAAAAAAAAAAAAAAAAAA==";
		DWORD len = 0;
		CryptBinaryToStringA(reinterpret_cast<const BYTE*>(&g), sizeof(g),
			CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &len);
		if (len == 0)
			return "AAAAAAAAAAAAAAAAAAAAAA==";
		std::string out(len, '\0');
		if (!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(&g), sizeof(g),
				CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, out.data(), &len))
			return "AAAAAAAAAAAAAAAAAAAAAA==";
		while (!out.empty() && (out.back() == '\0' || out.back() == '\n' || out.back() == '\r'))
			out.pop_back();
		return out;
	}
}

namespace
{
	struct SeedOnce
	{
		SeedOnce()
		{
			TrailToolsDetail::SeedDefaultCategories();
			TrailToolsBinds::SetDefaults();
		}
	};
	SeedOnce gSeed;
}
