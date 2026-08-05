#include "TrailToolsBinds.h"

#include "Globals.h"
#include "Settings.h"
#include "TrailToolsShared.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

namespace
{
	using TrailToolsBinds::Chord;
	using TrailToolsBinds::PlaceSlot;
	using TrailToolsBinds::State;
	using TrailToolsBinds::kPlaceSlots;

	State gBinds{};
	bool  gHeld[32]{}; /* edge detect for fixed actions + place slots */
	float gLastSampleX = 0.f, gLastSampleY = 0.f, gLastSampleZ = 0.f;
	bool  gHaveSample = false;

	bool KeyDown(int vk)
	{
		return (GetAsyncKeyState(vk) & 0x8000) != 0;
	}

	bool ModsMatch(const Chord& c)
	{
		const bool ctrl = KeyDown(VK_CONTROL) || KeyDown(VK_LCONTROL) || KeyDown(VK_RCONTROL);
		const bool shift = KeyDown(VK_SHIFT) || KeyDown(VK_LSHIFT) || KeyDown(VK_RSHIFT);
		const bool alt = KeyDown(VK_MENU) || KeyDown(VK_LMENU) || KeyDown(VK_RMENU);
		return ctrl == c.ctrl && shift == c.shift && alt == c.alt;
	}

	bool ChordDown(const Chord& c)
	{
		return c.vk != 0 && ModsMatch(c) && KeyDown(static_cast<int>(c.vk));
	}

	bool Edge(int idx, bool down)
	{
		const bool was = gHeld[idx];
		gHeld[idx] = down;
		return down && !was;
	}

	bool TypingBlocked()
	{
		const ImGuiIO& io = ImGui::GetIO();
		return io.WantTextInput;
	}

	void AppendPointAtFeet(bool requireMapMatch)
	{
		using namespace TrailToolsDetail;
		uint32_t mapId = 0;
		float x = 0.f, y = 0.f, z = 0.f;
		if (!ReadMumblePose(mapId, x, y, z))
		{
			SetStatus("No Mumble pose.");
			return;
		}
		if (gDraft.active.mapId == 0)
			gDraft.active.mapId = mapId;
		else if (requireMapMatch && gDraft.active.mapId != mapId)
		{
			SetStatus("Map mismatch — trail %u, you %u.", gDraft.active.mapId, mapId);
			return;
		}
		if (gDraft.active.type.empty() && gDraft.trailType[0])
			gDraft.active.type = gDraft.trailType;
		gDraft.active.points.push_back({ x, y, z });
		gDraft.selectedPoint = static_cast<int>(gDraft.active.points.size()) - 1;
		gDraft.trailDirty = true;
		gLastSampleX = x;
		gLastSampleY = y;
		gLastSampleZ = z;
		gHaveSample = true;
	}

	void SampleWhileRecording()
	{
		using namespace TrailToolsDetail;
		if (!gBinds.trailRecording || gBinds.trailPaused)
			return;
		uint32_t mapId = 0;
		float x = 0.f, y = 0.f, z = 0.f;
		if (!ReadMumblePose(mapId, x, y, z))
			return;
		if (gDraft.active.mapId == 0)
			gDraft.active.mapId = mapId;
		if (gDraft.active.mapId != mapId)
			return;
		constexpr float kMinDist = 1.25f; /* meters between auto samples */
		if (gHaveSample)
		{
			const float dx = x - gLastSampleX;
			const float dy = y - gLastSampleY;
			const float dz = z - gLastSampleZ;
			if (dx * dx + dy * dy + dz * dz < kMinDist * kMinDist)
				return;
		}
		gDraft.active.points.push_back({ x, y, z });
		gDraft.selectedPoint = static_cast<int>(gDraft.active.points.size()) - 1;
		gDraft.trailDirty = true;
		gLastSampleX = x;
		gLastSampleY = y;
		gLastSampleZ = z;
		gHaveSample = true;
	}

	struct VkName
	{
		unsigned    vk;
		const char* name;
	};

	const VkName kVkNames[] = {
		{ VK_NUMPAD0, "NUMPAD0" }, { VK_NUMPAD1, "NUMPAD1" }, { VK_NUMPAD2, "NUMPAD2" },
		{ VK_NUMPAD3, "NUMPAD3" }, { VK_NUMPAD4, "NUMPAD4" }, { VK_NUMPAD5, "NUMPAD5" },
		{ VK_NUMPAD6, "NUMPAD6" }, { VK_NUMPAD7, "NUMPAD7" }, { VK_NUMPAD8, "NUMPAD8" },
		{ VK_NUMPAD9, "NUMPAD9" }, { VK_MULTIPLY, "NUMPAD*" }, { VK_ADD, "NUMPAD+" },
		{ VK_SUBTRACT, "NUMPAD-" }, { VK_DIVIDE, "NUMPAD/" }, { VK_DECIMAL, "NUMPAD." },
		{ VK_BACK, "BACKSPACE" }, { VK_DELETE, "DELETE" }, { VK_INSERT, "INSERT" },
		{ VK_HOME, "HOME" }, { VK_END, "END" }, { VK_PRIOR, "PAGEUP" }, { VK_NEXT, "PAGEDOWN" },
		{ VK_SPACE, "SPACE" }, { VK_OEM_COMMA, "," }, { VK_OEM_PERIOD, "." },
		{ VK_OEM_MINUS, "-" }, { VK_OEM_PLUS, "=" }, { VK_OEM_1, ";" }, { VK_OEM_2, "/" },
		{ VK_F1, "F1" }, { VK_F2, "F2" }, { VK_F3, "F3" }, { VK_F4, "F4" },
		{ VK_F5, "F5" }, { VK_F6, "F6" }, { VK_F7, "F7" }, { VK_F8, "F8" },
		{ VK_F9, "F9" }, { VK_F10, "F10" }, { VK_F11, "F11" }, { VK_F12, "F12" },
	};

	unsigned VkFromName(const char* name)
	{
		if (!name || !*name)
			return 0;
		if (std::strlen(name) == 1)
		{
			const char c = name[0];
			if (c >= 'A' && c <= 'Z')
				return static_cast<unsigned>(c);
			if (c >= 'a' && c <= 'z')
				return static_cast<unsigned>(c - 'a' + 'A');
			if (c >= '0' && c <= '9')
				return static_cast<unsigned>(c);
		}
		for (const auto& e : kVkNames)
		{
			if (_stricmp(e.name, name) == 0)
				return e.vk;
		}
		return 0;
	}
}

TrailToolsBinds::State& TrailToolsBinds::Get()
{
	return gBinds;
}

const char* TrailToolsBinds::VkDisplayName(unsigned vk)
{
	if (vk == 0)
		return "";
	for (const auto& e : kVkNames)
	{
		if (e.vk == vk)
			return e.name;
	}
	static char buf[8];
	if (vk >= 'A' && vk <= 'Z')
	{
		buf[0] = static_cast<char>(vk);
		buf[1] = 0;
		return buf;
	}
	if (vk >= '0' && vk <= '9')
	{
		buf[0] = static_cast<char>(vk);
		buf[1] = 0;
		return buf;
	}
	std::snprintf(buf, sizeof(buf), "0x%02X", vk);
	return buf;
}

std::string TrailToolsBinds::FormatChord(const Chord& c)
{
	if (c.vk == 0)
		return "Unbound";
	std::string s;
	if (c.ctrl) s += "CTRL+";
	if (c.shift) s += "SHIFT+";
	if (c.alt) s += "ALT+";
	s += VkDisplayName(c.vk);
	return s;
}

bool TrailToolsBinds::ParseChord(const char* s, Chord& out)
{
	out = {};
	if (!s || !*s || _stricmp(s, "Unbound") == 0 || _stricmp(s, "none") == 0)
		return true;
	char buf[96]{};
	std::snprintf(buf, sizeof(buf), "%s", s);
	for (char* p = buf; *p; ++p)
		if (*p >= 'a' && *p <= 'z')
			*p = static_cast<char>(*p - 'a' + 'A');
	char* tok = buf;
	while (tok && *tok)
	{
		char* plus = std::strchr(tok, '+');
		if (plus)
			*plus = 0;
		if (std::strcmp(tok, "CTRL") == 0 || std::strcmp(tok, "CONTROL") == 0)
			out.ctrl = true;
		else if (std::strcmp(tok, "SHIFT") == 0)
			out.shift = true;
		else if (std::strcmp(tok, "ALT") == 0 || std::strcmp(tok, "MENU") == 0)
			out.alt = true;
		else
			out.vk = VkFromName(tok);
		tok = plus ? plus + 1 : nullptr;
	}
	return out.vk != 0;
}

void TrailToolsBinds::SetDefaults()
{
	gBinds = {};
	ParseChord("CTRL+NUMPAD*", gBinds.trailStart);
	ParseChord("CTRL+NUMPAD/", gBinds.trailPause);
	ParseChord("CTRL+NUMPAD+", gBinds.trailSection);
	ParseChord("CTRL+NUMPAD-", gBinds.trailDeleteSeg);
	ParseChord("CTRL+DELETE", gBinds.markerDelete);
	const unsigned pads[kPlaceSlots] = {
		VK_NUMPAD1, VK_NUMPAD2, VK_NUMPAD3, VK_NUMPAD4, VK_NUMPAD5,
		VK_NUMPAD6, VK_NUMPAD7, VK_NUMPAD8, VK_NUMPAD9, VK_NUMPAD0,
	};
	for (int i = 0; i < kPlaceSlots; ++i)
	{
		gBinds.place[i].chord.ctrl = true;
		gBinds.place[i].chord.vk = pads[i];
		std::snprintf(gBinds.place[i].label, sizeof(gBinds.place[i].label), "Marker %d", i + 1);
	}
}

void TrailToolsBinds::ActionTrailStart()
{
	using namespace TrailToolsDetail;
	EnsureWorkspace();
	if (!gBinds.trailRecording)
	{
		gBinds.trailRecording = true;
		gBinds.trailPaused = false;
		gHaveSample = false;
		if (gDraft.active.type.empty() && gDraft.trailType[0])
			gDraft.active.type = gDraft.trailType;
		AppendPointAtFeet(false);
		SetStatus("Recording trail… (%zu pts).", gDraft.active.points.size());
		return;
	}
	if (gBinds.trailPaused)
	{
		gBinds.trailPaused = false;
		SetStatus("Trail recording resumed.");
		return;
	}
	AppendPointAtFeet(true);
	SetStatus("Keyframe #%zu.", gDraft.active.points.size());
}

void TrailToolsBinds::ActionTrailPause()
{
	using namespace TrailToolsDetail;
	if (!gBinds.trailRecording)
	{
		SetStatus("Not recording.");
		return;
	}
	gBinds.trailPaused = !gBinds.trailPaused;
	SetStatus(gBinds.trailPaused ? "Trail recording paused." : "Trail recording resumed.");
}

void TrailToolsBinds::ActionTrailSection()
{
	using namespace TrailToolsDetail;
	gDraft.active.points.push_back({ 0.f, 0.f, 0.f });
	gDraft.selectedPoint = static_cast<int>(gDraft.active.points.size()) - 1;
	gDraft.trailDirty = true;
	gHaveSample = false;
	SetStatus("Section break added.");
}

void TrailToolsBinds::ActionTrailDeleteSeg()
{
	using namespace TrailToolsDetail;
	if (gDraft.active.points.empty())
	{
		SetStatus("No trail points.");
		return;
	}
	int& sel = gDraft.selectedPoint;
	if (sel >= 0 && sel < static_cast<int>(gDraft.active.points.size()))
		gDraft.active.points.erase(gDraft.active.points.begin() + sel);
	else
	{
		gDraft.active.points.pop_back();
		sel = static_cast<int>(gDraft.active.points.size()) - 1;
	}
	if (sel >= static_cast<int>(gDraft.active.points.size()))
		sel = static_cast<int>(gDraft.active.points.size()) - 1;
	gDraft.trailDirty = true;
	SetStatus("Deleted trail segment (%zu left).", gDraft.active.points.size());
}

void TrailToolsBinds::ActionPlaceMarker(int slotIndex)
{
	using namespace TrailToolsDetail;
	EnsureWorkspace();
	uint32_t mapId = 0;
	float x = 0.f, y = 0.f, z = 0.f;
	if (!ReadMumblePose(mapId, x, y, z))
	{
		SetStatus("No Mumble pose.");
		return;
	}
	const char* type = gDraft.markerType;
	if (slotIndex >= 0 && slotIndex < kPlaceSlots && gBinds.place[slotIndex].type[0])
		type = gBinds.place[slotIndex].type;
	if (!type || !type[0])
	{
		SetStatus("Set a marker type for this slot (Keybinds tab).");
		return;
	}
	DraftPoi p;
	p.mapId = mapId;
	p.x = x;
	p.y = y;
	p.z = z;
	p.type = type;
	p.guid = MakeGuidBase64();
	gDraft.pois.push_back(std::move(p));
	gDraft.selectedPoi = static_cast<int>(gDraft.pois.size()) - 1;
	const char* lab = (slotIndex >= 0 && slotIndex < kPlaceSlots && gBinds.place[slotIndex].label[0])
		? gBinds.place[slotIndex].label
		: type;
	SetStatus("Placed %s (#%zu).", lab, gDraft.pois.size());
}

void TrailToolsBinds::ActionDeleteMarker()
{
	using namespace TrailToolsDetail;
	if (gDraft.pois.empty())
	{
		SetStatus("No markers.");
		return;
	}
	int& sel = gDraft.selectedPoi;
	if (sel < 0 || sel >= static_cast<int>(gDraft.pois.size()))
		sel = static_cast<int>(gDraft.pois.size()) - 1;
	gDraft.pois.erase(gDraft.pois.begin() + sel);
	if (sel >= static_cast<int>(gDraft.pois.size()))
		sel = static_cast<int>(gDraft.pois.size()) - 1;
	SetStatus("Deleted marker (%zu left).", gDraft.pois.size());
}

void TrailToolsBinds::Poll()
{
	/* Capture mode: next non-modifier key with current mods becomes the bind. */
	if (gBinds.captureTarget >= 0)
	{
		const bool ctrl = KeyDown(VK_CONTROL) || KeyDown(VK_LCONTROL) || KeyDown(VK_RCONTROL);
		const bool shift = KeyDown(VK_SHIFT) || KeyDown(VK_LSHIFT) || KeyDown(VK_RSHIFT);
		const bool alt = KeyDown(VK_MENU) || KeyDown(VK_LMENU) || KeyDown(VK_RMENU);
		for (int vk = 1; vk < 256; ++vk)
		{
			if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
				vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT ||
				vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU ||
				vk == VK_LWIN || vk == VK_RWIN || vk == VK_CAPITAL || vk == VK_NUMLOCK ||
				vk == VK_SCROLL || vk == VK_ESCAPE)
				continue;
			if (!KeyDown(vk))
				continue;
			Chord c;
			c.ctrl = ctrl;
			c.shift = shift;
			c.alt = alt;
			c.vk = static_cast<unsigned>(vk);
			const int t = gBinds.captureTarget;
			if (t == 0) gBinds.trailStart = c;
			else if (t == 1) gBinds.trailPause = c;
			else if (t == 2) gBinds.trailSection = c;
			else if (t == 3) gBinds.trailDeleteSeg = c;
			else if (t == 4) gBinds.markerDelete = c;
			else if (t >= 10 && t < 10 + kPlaceSlots)
				gBinds.place[t - 10].chord = c;
			gBinds.captureTarget = -1;
			Settings::SetDirty();
			/* swallow until release — clear held so we don't fire immediately */
			std::memset(gHeld, 0, sizeof(gHeld));
			return;
		}
		if (KeyDown(VK_ESCAPE))
			gBinds.captureTarget = -1;
		return;
	}

	SampleWhileRecording();

	if (TypingBlocked())
		return;

	if (Edge(0, ChordDown(gBinds.trailStart)))
		ActionTrailStart();
	if (Edge(1, ChordDown(gBinds.trailPause)))
		ActionTrailPause();
	if (Edge(2, ChordDown(gBinds.trailSection)))
		ActionTrailSection();
	if (Edge(3, ChordDown(gBinds.trailDeleteSeg)))
		ActionTrailDeleteSeg();
	if (Edge(4, ChordDown(gBinds.markerDelete)))
		ActionDeleteMarker();
	for (int i = 0; i < kPlaceSlots; ++i)
	{
		if (Edge(10 + i, ChordDown(gBinds.place[i].chord)))
			ActionPlaceMarker(i);
	}
}

std::string TrailToolsBinds::Serialize()
{
	std::string o;
	auto add = [&](const char* key, const Chord& c) {
		o += key;
		o += '=';
		o += FormatChord(c);
		o += ';';
	};
	add("start", gBinds.trailStart);
	add("pause", gBinds.trailPause);
	add("section", gBinds.trailSection);
	add("delseg", gBinds.trailDeleteSeg);
	add("delmark", gBinds.markerDelete);
	for (int i = 0; i < kPlaceSlots; ++i)
	{
		char k[32]{};
		std::snprintf(k, sizeof(k), "p%d", i);
		add(k, gBinds.place[i].chord);
		std::snprintf(k, sizeof(k), "pt%d", i);
		o += k;
		o += '=';
		o += gBinds.place[i].type;
		o += ';';
		std::snprintf(k, sizeof(k), "pl%d", i);
		o += k;
		o += '=';
		o += gBinds.place[i].label;
		o += ';';
	}
	return o;
}

void TrailToolsBinds::Deserialize(const char* s)
{
	SetDefaults();
	if (!s || !*s)
		return;
	char buf[4096]{};
	std::snprintf(buf, sizeof(buf), "%s", s);
	char* p = buf;
	while (p && *p)
	{
		char* semi = std::strchr(p, ';');
		if (semi)
			*semi = 0;
		char* eq = std::strchr(p, '=');
		if (eq)
		{
			*eq = 0;
			const char* key = p;
			const char* val = eq + 1;
			Chord c;
			if (std::strcmp(key, "start") == 0 && ParseChord(val, c))
				gBinds.trailStart = c;
			else if (std::strcmp(key, "pause") == 0 && ParseChord(val, c))
				gBinds.trailPause = c;
			else if (std::strcmp(key, "section") == 0 && ParseChord(val, c))
				gBinds.trailSection = c;
			else if (std::strcmp(key, "delseg") == 0 && ParseChord(val, c))
				gBinds.trailDeleteSeg = c;
			else if (std::strcmp(key, "delmark") == 0 && ParseChord(val, c))
				gBinds.markerDelete = c;
			else if (key[0] == 'p' && key[1] >= '0' && key[1] <= '9' && key[2] == 0)
			{
				const int i = key[1] - '0';
				if (i >= 0 && i < kPlaceSlots && ParseChord(val, c))
					gBinds.place[i].chord = c;
			}
			else if (key[0] == 'p' && key[1] == 't' && key[2] >= '0' && key[2] <= '9' && !key[3])
			{
				const int i = key[2] - '0';
				if (i >= 0 && i < kPlaceSlots)
					std::snprintf(gBinds.place[i].type, sizeof(gBinds.place[i].type), "%s", val);
			}
			else if (key[0] == 'p' && key[1] == 'l' && key[2] >= '0' && key[2] <= '9' && !key[3])
			{
				const int i = key[2] - '0';
				if (i >= 0 && i < kPlaceSlots)
					std::snprintf(gBinds.place[i].label, sizeof(gBinds.place[i].label), "%s", val);
			}
		}
		p = semi ? semi + 1 : nullptr;
	}
}
