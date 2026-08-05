/* Host/Wine smoke: Trail Tools .trl + XML roundtrip (no Nexus / Globals). */
#include "TrailToolsTrl.h"
#include "TrailToolsXml.h"
#include "TrailToolsShared.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

static int Fail(const char* msg)
{
	std::fprintf(stderr, "FAIL: %s\n", msg);
	return 1;
}

static void SeedMinimal()
{
	using namespace TrailToolsDetail;
	std::snprintf(gDraft.packName, sizeof(gDraft.packName), "ExamplePack");
	std::snprintf(gDraft.displayName, sizeof(gDraft.displayName), "Example Pack");
	gDraft.root = {};
	gDraft.root.name = "examplepack";
	gDraft.root.displayName = "Example Pack";
	CategoryNode markers;
	markers.name = "m";
	markers.displayName = "Markers";
	CategoryNode exm;
	exm.name = "exm";
	exm.displayName = "Example Marker";
	exm.iconFile = "Data/ExamplePack/Markers/ExampleMarker.png";
	markers.children.push_back(exm);
	gDraft.root.children.push_back(markers);
	CategoryNode trails;
	trails.name = "t";
	trails.displayName = "Trails";
	CategoryNode extrail;
	extrail.name = "extrail";
	extrail.displayName = "Example Trail";
	extrail.texture = "Data/ExamplePack/Markers/Trail.png";
	trails.children.push_back(extrail);
	gDraft.root.children.push_back(trails);
	std::snprintf(gDraft.markerType, sizeof(gDraft.markerType), "examplepack.m.exm");
	std::snprintf(gDraft.trailType, sizeof(gDraft.trailType), "examplepack.t.extrail");
}

int main()
{
	using namespace TrailToolsDetail;
	SeedMinimal();

	DraftPoi p;
	p.mapId = 50;
	p.x = -563.204f;
	p.y = 26.6099f;
	p.z = 94.5559f;
	p.type = gDraft.markerType;
	p.guid = "fEvVAceiaUanEsb/Rrea6A==";
	gDraft.pois.push_back(p);

	gDraft.active.mapId = 50;
	gDraft.active.type = gDraft.trailType;
	gDraft.active.fileRel = "Data/ExamplePack/Trails/Trail.trl";
	gDraft.active.points = {
		{ 1.f, 2.f, 3.f },
		{ 4.f, 5.f, 6.f },
		{ 0.f, 0.f, 0.f },
		{ 7.f, 8.f, 9.f },
		{ 10.f, 11.f, 12.f },
	};

	const std::string xml = TrailToolsXml::EmitOverlayData(gDraft);
	if (xml.find("<POI MapID=\"50\"") == std::string::npos)
		return Fail("XML missing POI");
	if (xml.find("examplepack.m.exm") == std::string::npos)
		return Fail("XML missing marker type");
	if (xml.find("<Trail type=") == std::string::npos)
		return Fail("XML missing Trail");
	if (xml.find("GUID=\"fEvVAceiaUanEsb/Rrea6A==\"") == std::string::npos)
		return Fail("XML missing GUID");
	if (xml.find("iconFile=\"Data/ExamplePack/Markers/ExampleMarker.png\"") == std::string::npos)
		return Fail("XML missing marker iconFile");

	wchar_t tmp[MAX_PATH]{};
	GetTempPathW(MAX_PATH, tmp);
	std::wstring trl = tmp;
	trl += L"gw2igh_tt_test.trl";
	if (!TrailToolsTrl::Write(trl, 50, gDraft.active.points))
		return Fail("trl write");
	uint32_t mid = 0;
	std::vector<PathingTrails::WorldPoint> pts;
	if (!TrailToolsTrl::Read(trl, mid, pts))
		return Fail("trl read");
	if (mid != 50 || pts.size() != gDraft.active.points.size())
		return Fail("trl map/count mismatch");
	for (size_t i = 0; i < pts.size(); ++i)
	{
		if (pts[i].x != gDraft.active.points[i].x ||
			pts[i].y != gDraft.active.points[i].y ||
			pts[i].z != gDraft.active.points[i].z)
			return Fail("trl point mismatch");
	}
	DeleteFileW(trl.c_str());

	std::printf("OK trailtools roundtrip (%zu xml bytes, %zu trl pts)\n",
		xml.size(), pts.size());
	return 0;
}

namespace TrailToolsDetail
{
	DraftPack gDraft{};
	bool gPlaceOnce = false;
	bool gFocus = false;
	int gTab = 0;
}
