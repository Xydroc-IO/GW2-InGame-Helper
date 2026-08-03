#include "CharacterProfiles.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "MumbleIdentity.h"
#include "Settings.h"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace
{
	constexpr int kVersion = 1;

	struct Profile
	{
		bool helper = false;
		bool pathing = false;
		bool wallet = false;
		bool notes = false;
		bool account = false;
		float fontScale = 1.f;
		float winX = 0.f;
		float winY = 0.f;
		float winW = 0.f;
		float winH = 0.f;
		bool hasPos = false;
		bool hasSize = false;
	};

	std::mutex gMu;
	std::unordered_map<std::string, Profile> gProfiles;
	std::string gActive;
	bool gDirty = false;
	DWORD gLastSaveAttempt = 0;

	std::wstring PathW()
	{
		return AddonPaths::DataDir() + L"\\profiles.json";
	}

	bool WriteUtf8File(const std::wstring& path, const std::string& data)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
		CloseHandle(h);
		return ok && written == data.size();
	}

	std::string ReadUtf8File(const std::wstring& path)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return {};
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 2 * 1024 * 1024)
		{
			CloseHandle(h);
			return {};
		}
		std::string out(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD read = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
		CloseHandle(h);
		if (!ok || read != out.size())
			return {};
		return out;
	}

	std::string EscapeJson(const std::string& s)
	{
		std::string o;
		o.reserve(s.size() + 8);
		for (char c : s)
		{
			if (c == '\\' || c == '"')
			{
				o.push_back('\\');
				o.push_back(c);
			}
			else if (c == '\n')
				o += "\\n";
			else if (c == '\r')
				o += "\\r";
			else if (c == '\t')
				o += "\\t";
			else
				o.push_back(c);
		}
		return o;
	}

	Profile CaptureFromGlobals()
	{
		Profile p;
		p.helper = G::ShowWiki;
		p.pathing = G::ShowTekkitGuides;
		p.wallet = G::ShowWallet;
		p.notes = G::ShowNotes;
		p.account = G::ShowAccount;
		p.fontScale = G::FontScale;
		p.winX = G::WindowPosX;
		p.winY = G::WindowPosY;
		p.winW = G::WindowWidth;
		p.winH = G::WindowHeight;
		p.hasPos = G::HasSavedPos;
		p.hasSize = G::HasSavedSize;
		return p;
	}

	void ApplyToGlobals(const Profile& p)
	{
		G::ShowWiki = p.helper;
		G::ShowTekkitGuides = p.pathing;
		G::ShowWallet = p.wallet;
		G::ShowNotes = p.notes;
		G::ShowAccount = p.account;

		G::FontScale = p.fontScale;
		if (G::FontScale < 0.75f) G::FontScale = 0.75f;
		if (G::FontScale > 2.f) G::FontScale = 2.f;

		if (p.hasPos)
		{
			G::WindowPosX = p.winX;
			G::WindowPosY = p.winY;
			G::HasSavedPos = true;
		}
		if (p.hasSize)
		{
			G::WindowWidth = p.winW;
			G::WindowHeight = p.winH;
			G::HasSavedSize = true;
		}
		Settings::SetDirty();
	}

	std::string SerializeLocked()
	{
		std::string out;
		out += "{\n  \"version\": ";
		out += std::to_string(kVersion);
		out += ",\n  \"profiles\": {\n";
		bool first = true;
		for (const auto& kv : gProfiles)
		{
			if (!first)
				out += ",\n";
			first = false;
			const Profile& p = kv.second;
			out += "    \"";
			out += EscapeJson(kv.first);
			out += "\": {\n";
			out += "      \"panels\": [";
			bool pf = true;
			auto add = [&](bool on, const char* id) {
				if (!on) return;
				if (!pf) out += ", ";
				pf = false;
				out += "\"";
				out += id;
				out += "\"";
			};
			add(p.helper, "helper");
			add(p.pathing, "pathing");
			add(p.wallet, "wallet");
			add(p.notes, "notes");
			add(p.account, "account");
			out += "],\n";
			char buf[128];
			std::snprintf(buf, sizeof(buf),
				"      \"font_scale\": %.4f,\n"
				"      \"window\": { \"x\": %.2f, \"y\": %.2f, \"w\": %.2f, \"h\": %.2f, "
				"\"has_pos\": %s, \"has_size\": %s }\n",
				p.fontScale, p.winX, p.winY, p.winW, p.winH,
				p.hasPos ? "true" : "false",
				p.hasSize ? "true" : "false");
			out += buf;
			out += "    }";
		}
		out += "\n  }\n}\n";
		return out;
	}

	bool JsonFindBoolNear(const std::string& json, size_t from, size_t limit, const char* key)
	{
		const std::string pat = std::string("\"") + key + "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos || k >= limit)
			return false;
		k = json.find(':', k + pat.size());
		if (k == std::string::npos || k >= limit)
			return false;
		++k;
		while (k < limit && (json[k] == ' ' || json[k] == '\t'))
			++k;
		return json.compare(k, 4, "true") == 0;
	}

	float JsonFloatNear(const std::string& json, size_t from, size_t limit, const char* key, float def)
	{
		const std::string pat = std::string("\"") + key + "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos || k >= limit)
			return def;
		k = json.find(':', k + pat.size());
		if (k == std::string::npos || k >= limit)
			return def;
		++k;
		while (k < limit && (json[k] == ' ' || json[k] == '\t'))
			++k;
		return static_cast<float>(std::atof(json.c_str() + k));
	}

	bool PanelListed(const std::string& panelsBlock, const char* id)
	{
		const std::string needle = std::string("\"") + id + "\"";
		return panelsBlock.find(needle) != std::string::npos;
	}

	void ParseProfiles(const std::string& raw)
	{
		gProfiles.clear();
		size_t profilesKey = raw.find("\"profiles\"");
		if (profilesKey == std::string::npos)
			return;
		size_t obj = raw.find('{', profilesKey);
		if (obj == std::string::npos)
			return;

		size_t i = obj + 1;
		while (i < raw.size())
		{
			while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\n' || raw[i] == '\r' ||
				raw[i] == '\t' || raw[i] == ','))
				++i;
			if (i >= raw.size() || raw[i] == '}')
				break;
			if (raw[i] != '"')
				break;
			++i;
			std::string name;
			while (i < raw.size())
			{
				const char c = raw[i++];
				if (c == '\\' && i < raw.size())
				{
					name.push_back(raw[i++]);
					continue;
				}
				if (c == '"')
					break;
				name.push_back(c);
			}
			size_t colon = raw.find(':', i);
			if (colon == std::string::npos)
				break;
			size_t brace = raw.find('{', colon);
			if (brace == std::string::npos)
				break;
			int depth = 0;
			size_t end = brace;
			bool inStr = false, esc = false;
			for (; end < raw.size(); ++end)
			{
				const char c = raw[end];
				if (inStr)
				{
					if (esc) esc = false;
					else if (c == '\\') esc = true;
					else if (c == '"') inStr = false;
					continue;
				}
				if (c == '"') { inStr = true; continue; }
				if (c == '{') ++depth;
				else if (c == '}')
				{
					--depth;
					if (depth == 0)
					{
						++end;
						break;
					}
				}
			}
			if (depth != 0 || name.empty())
			{
				i = end;
				continue;
			}

			Profile p;
			const size_t panelsAt = raw.find("\"panels\"", brace);
			std::string panelsBlock;
			if (panelsAt != std::string::npos && panelsAt < end)
			{
				const size_t lb = raw.find('[', panelsAt);
				const size_t rb = (lb != std::string::npos) ? raw.find(']', lb) : std::string::npos;
				if (lb != std::string::npos && rb != std::string::npos && rb < end)
					panelsBlock = raw.substr(lb, rb - lb + 1);
			}
			p.helper = PanelListed(panelsBlock, "helper");
			p.pathing = PanelListed(panelsBlock, "pathing");
			p.wallet = PanelListed(panelsBlock, "wallet");
			p.notes = PanelListed(panelsBlock, "notes");
			p.account = PanelListed(panelsBlock, "account");
			p.fontScale = JsonFloatNear(raw, brace, end, "font_scale", 1.f);
			p.winX = JsonFloatNear(raw, brace, end, "x", 0.f);
			p.winY = JsonFloatNear(raw, brace, end, "y", 0.f);
			p.winW = JsonFloatNear(raw, brace, end, "w", 0.f);
			p.winH = JsonFloatNear(raw, brace, end, "h", 0.f);
			p.hasPos = JsonFindBoolNear(raw, brace, end, "has_pos");
			p.hasSize = JsonFindBoolNear(raw, brace, end, "has_size");
			gProfiles[name] = p;
			i = end;
		}
	}
}

void CharacterProfiles::Load()
{
	std::lock_guard<std::mutex> lock(gMu);
	gProfiles.clear();
	gActive.clear();
	gDirty = false;
	const std::string raw = ReadUtf8File(PathW());
	if (!raw.empty())
		ParseProfiles(raw);
}

void CharacterProfiles::Save(bool force)
{
	std::string payload;
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (!force && !gDirty)
			return;
		const DWORD now = GetTickCount();
		if (!force && gLastSaveAttempt != 0 && (now - gLastSaveAttempt) < 1500u)
			return;
		gLastSaveAttempt = now;
		payload = SerializeLocked();
		gDirty = false;
	}
	CreateDirectoryW(AddonPaths::DataDir().c_str(), nullptr);
	WriteUtf8File(PathW(), payload);
}

void CharacterProfiles::CaptureCurrent()
{
	std::lock_guard<std::mutex> lock(gMu);
	if (gActive.empty())
	{
		const std::string name = MumbleIdentity::CharacterNameStr();
		if (name.empty())
			return;
		gActive = name;
	}
	gProfiles[gActive] = CaptureFromGlobals();
	gDirty = true;
}

void CharacterProfiles::ApplyProfile(const char* characterName)
{
	if (!characterName || !characterName[0])
		return;
	Profile p;
	bool found = false;
	{
		std::lock_guard<std::mutex> lock(gMu);
		gActive = characterName;
		auto it = gProfiles.find(gActive);
		if (it == gProfiles.end())
		{
			/* First time on this character — snapshot current layout, don't thrash UI. */
			gProfiles[gActive] = CaptureFromGlobals();
			gDirty = true;
			return;
		}
		p = it->second;
		found = true;
	}
	if (found)
		ApplyToGlobals(p);
}

void CharacterProfiles::Tick()
{
	if (MumbleIdentity::TakeChanged())
	{
		const std::string next = MumbleIdentity::CharacterNameStr();
		{
			std::lock_guard<std::mutex> lock(gMu);
			if (!gActive.empty())
			{
				gProfiles[gActive] = CaptureFromGlobals();
				gDirty = true;
			}
		}
		Save(true);

		if (next.empty())
		{
			std::lock_guard<std::mutex> lock(gMu);
			gActive.clear();
			return;
		}
		ApplyProfile(next.c_str());
		return;
	}

	/* Keep the active profile in sync with live panel toggles. */
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (!gActive.empty())
		{
			Profile now = CaptureFromGlobals();
			auto it = gProfiles.find(gActive);
			if (it == gProfiles.end() ||
				it->second.helper != now.helper ||
				it->second.pathing != now.pathing ||
				it->second.wallet != now.wallet ||
				it->second.notes != now.notes ||
				it->second.account != now.account ||
				it->second.fontScale != now.fontScale ||
				it->second.hasPos != now.hasPos ||
				it->second.hasSize != now.hasSize ||
				it->second.winX != now.winX ||
				it->second.winY != now.winY ||
				it->second.winW != now.winW ||
				it->second.winH != now.winH)
			{
				gProfiles[gActive] = now;
				gDirty = true;
			}
		}
	}
	Save(false);
}

const char* CharacterProfiles::ActiveCharacter()
{
	return gActive.c_str();
}
