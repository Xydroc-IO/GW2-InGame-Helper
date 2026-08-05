#include "TrailToolsDraftStyle.h"

#include "Globals.h"
#include "PathingIndex.h"
#include "TrailToolsShared.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace
{
	struct Pending
	{
		std::string id;
		std::vector<uint8_t> bytes;
	};

	std::mutex gMu;
	std::vector<Pending> gPending;
	std::unordered_map<std::string, std::vector<uint8_t>> gRetain;

	std::wstring RelToAbs(const std::string& rel)
	{
		std::wstring path = TrailToolsDetail::PackDir();
		path.push_back(L'\\');
		for (char c : rel)
		{
			if (c == '/')
				path.push_back(L'\\');
			else
				path.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
		}
		return path;
	}

	bool ReadFileBytes(const std::wstring& path, std::vector<uint8_t>& out)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 8 * 1024 * 1024)
		{
			CloseHandle(h);
			return false;
		}
		out.resize(static_cast<size_t>(sz.QuadPart));
		DWORD read = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
		CloseHandle(h);
		if (!ok || read != out.size())
		{
			out.clear();
			return false;
		}
		return true;
	}

	void QueueRel(const std::string& rel, char* idOut, size_t idCap)
	{
		if (rel.empty() || !idOut || idCap == 0)
			return;
		const std::string tid = PathingDetail::IconTextureId(rel);
		std::snprintf(idOut, idCap, "%s", tid.c_str());
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (gRetain.count(tid))
				return;
			for (const Pending& p : gPending)
			{
				if (p.id == tid)
					return;
			}
		}
		std::vector<uint8_t> bytes;
		if (!ReadFileBytes(RelToAbs(rel), bytes) || bytes.empty())
			return;
		std::lock_guard<std::mutex> lock(gMu);
		gPending.push_back(Pending{ tid, std::move(bytes) });
	}

	void FillFromNode(const TrailToolsDetail::CategoryNode* n,
		TrailToolsDraftStyle::Resolved& r)
	{
		if (!n)
			return;
		if (!n->texture.empty())
			r.textureRel = n->texture;
		if (!n->iconFile.empty())
			r.iconRel = n->iconFile;
		if (n->color != 0)
			r.color = n->color;
		if (n->trailScale > 0.f)
			r.trailScale = n->trailScale;
		if (n->iconSize > 0.f)
			r.iconSize = n->iconSize;
		if (n->alpha > 0.f)
			r.alpha = n->alpha;
		r.fadeNear = n->fadeNear;
		r.fadeFar = n->fadeFar;
	}
}

void TrailToolsDraftStyle::BeginFrame()
{
	if (!G::API || !G::API->Textures_GetOrCreateFromMemory)
		return;

	std::vector<std::string> drop;
	{
		std::lock_guard<std::mutex> lock(gMu);
		for (const auto& kv : gRetain)
			drop.push_back(kv.first);
	}
	for (const std::string& id : drop)
	{
		Texture_t* tex = G::API->Textures_Get ? G::API->Textures_Get(id.c_str()) : nullptr;
		if (!(tex && tex->Resource))
			continue;
		std::lock_guard<std::mutex> lock(gMu);
		gRetain.erase(id);
	}

	for (int n = 0; n < 24; ++n)
	{
		Pending icon;
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (gPending.empty())
				break;
			icon = std::move(gPending.front());
			gPending.erase(gPending.begin());
		}
		if (icon.bytes.empty())
			continue;
		if (G::API->Textures_Get(icon.id.c_str()) &&
			G::API->Textures_Get(icon.id.c_str())->Resource)
			continue;
		const uint8_t* data = nullptr;
		uint64_t size = 0;
		{
			std::lock_guard<std::mutex> lock(gMu);
			auto& slot = gRetain[icon.id];
			if (slot.empty())
				slot = std::move(icon.bytes);
			data = slot.data();
			size = static_cast<uint64_t>(slot.size());
		}
		if (data && size)
			G::API->Textures_GetOrCreateFromMemory(
				icon.id.c_str(), const_cast<uint8_t*>(data), size);
	}
}

TrailToolsDraftStyle::Resolved TrailToolsDraftStyle::ResolveTrail()
{
	using namespace TrailToolsDetail;
	Resolved r;
	const std::string want = gDraft.trailType[0]
		? std::string(gDraft.trailType) : (RootCategoryName() + ".t.extrail");
	FillFromNode(FindCategoryByPath(gDraft.root, want), r);
	if (!r.textureRel.empty())
		QueueRel(r.textureRel, r.textureId, sizeof(r.textureId));
	return r;
}

TrailToolsDraftStyle::Resolved TrailToolsDraftStyle::ResolveMarkerType(
	const std::string& typePath)
{
	using namespace TrailToolsDetail;
	Resolved r;
	const std::string want = !typePath.empty() ? typePath
		: (gDraft.markerType[0] ? std::string(gDraft.markerType)
			: RootCategoryName() + ".m.exm");
	FillFromNode(FindCategoryByPath(gDraft.root, want), r);
	if (!r.iconRel.empty())
		QueueRel(r.iconRel, r.iconId, sizeof(r.iconId));
	return r;
}

PathingTrails::WorldSnippet TrailToolsDraftStyle::BuildActiveSnippet()
{
	using namespace TrailToolsDetail;
	PathingTrails::WorldSnippet snip;
	if (gDraft.active.points.size() < 2)
		return snip;
	const Resolved sty = ResolveTrail();
	snip.color = sty.color ? sty.color : 0xFFFFFFFFu;
	snip.alpha = sty.alpha > 0.f ? sty.alpha : 1.f;
	snip.trailScale = sty.trailScale > 0.f ? sty.trailScale : 1.f;
	snip.fadeNear = sty.fadeNear;
	snip.fadeFar = sty.fadeFar;
	if (sty.textureId[0])
		std::snprintf(snip.textureId, sizeof(snip.textureId), "%s", sty.textureId);
	std::snprintf(snip.label, sizeof(snip.label), "draft:%s",
		gDraft.trailType[0] ? gDraft.trailType : "trail");
	snip.points = gDraft.active.points;
	return snip;
}

PathingTrails::Marker TrailToolsDraftStyle::BuildDraftMarker(
	const TrailToolsDetail::DraftPoi& poi)
{
	PathingTrails::Marker m;
	m.mapId = poi.mapId;
	m.world = { poi.x, poi.y, poi.z };
	m.pos = {}; /* continent filled by compass caller if needed */
	std::snprintf(m.label, sizeof(m.label), "%s", poi.type.c_str());
	std::snprintf(m.guid, sizeof(m.guid), "%s", poi.guid.c_str());
	const Resolved sty = ResolveMarkerType(poi.type);
	m.color = sty.color ? sty.color : 0xFFFFC828u;
	m.alpha = sty.alpha > 0.f ? sty.alpha : 1.f;
	m.iconSize = sty.iconSize > 0.f ? sty.iconSize : 1.f;
	m.mapDisplaySize = 20.f;
	m.heightOffset = 1.5f;
	if (sty.iconId[0])
		std::snprintf(m.iconId, sizeof(m.iconId), "%s", sty.iconId);
	return m;
}
