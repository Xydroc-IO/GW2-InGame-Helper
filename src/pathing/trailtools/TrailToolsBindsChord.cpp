#include "TrailToolsBinds.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>

namespace
{
	using TrailToolsBinds::Chord;
	using TrailToolsBinds::kPlaceSlots;

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
	State& gBinds = Get();
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

std::string TrailToolsBinds::Serialize()
{
	State& gBinds = Get();
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
	State& gBinds = Get();
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
