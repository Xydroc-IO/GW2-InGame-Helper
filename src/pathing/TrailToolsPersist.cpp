#include "TrailToolsShared.h"
#include "TrailToolsXml.h"

#include <cstdio>
#include <string>
#include <vector>

#include <windows.h>

namespace
{
	std::wstring SessionPath()
	{
		return TrailToolsDetail::PackDir() + L"\\_draft_session.xml";
	}

	bool WriteAll(const std::wstring& path, const std::string& data)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD w = 0;
		const BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &w, nullptr);
		CloseHandle(h);
		return ok && w == data.size();
	}

	bool ReadAll(const std::wstring& path, std::string& out)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 16 * 1024 * 1024)
		{
			CloseHandle(h);
			return false;
		}
		out.resize(static_cast<size_t>(sz.QuadPart));
		DWORD r = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &r, nullptr);
		CloseHandle(h);
		return ok && r == out.size();
	}

	std::string Attr(const std::string& tag, const char* key)
	{
		const std::string needle = std::string(key) + "=\"";
		size_t p = tag.find(needle);
		if (p == std::string::npos)
			return {};
		p += needle.size();
		size_t e = tag.find('"', p);
		if (e == std::string::npos)
			return {};
		return tag.substr(p, e - p);
	}
}

bool TrailToolsDetail::SaveDraftSession()
{
	EnsureWorkspace();
	/* Persist OverlayData XML + a small active-trail sidecar in comments. */
	std::string xml = TrailToolsXml::EmitOverlayData(gDraft);
	xml += "<!-- ACTIVE map=\"";
	xml += std::to_string(gDraft.active.mapId);
	xml += "\" file=\"";
	xml += gDraft.active.fileRel;
	xml += "\" type=\"";
	xml += gDraft.active.type;
	xml += "\" pts=\"";
	xml += std::to_string(gDraft.active.points.size());
	xml += "\" -->\n";
	if (!WriteAll(SessionPath(), xml))
	{
		SetStatus("Failed to save draft session.");
		return false;
	}
	SetStatus("Draft session saved.");
	return true;
}

bool TrailToolsDetail::LoadDraftSession()
{
	std::string xml;
	if (!ReadAll(SessionPath(), xml))
	{
		SetStatus("No draft session on disk.");
		return false;
	}
	/* Minimal reload: keep categories via reseed + re-parse POIs from XML tags. */
	gDraft.pois.clear();
	size_t pos = 0;
	while ((pos = xml.find("<POI ", pos)) != std::string::npos)
	{
		const size_t end = xml.find("/>", pos);
		if (end == std::string::npos)
			break;
		const std::string tag = xml.substr(pos, end - pos);
		DraftPoi p;
		p.mapId = static_cast<uint32_t>(std::atoi(Attr(tag, "MapID").c_str()));
		p.x = static_cast<float>(std::atof(Attr(tag, "xpos").c_str()));
		p.y = static_cast<float>(std::atof(Attr(tag, "ypos").c_str()));
		p.z = static_cast<float>(std::atof(Attr(tag, "zpos").c_str()));
		p.type = Attr(tag, "type");
		p.guid = Attr(tag, "GUID");
		if (p.guid.empty())
			p.guid = Attr(tag, "guid");
		p.behavior = std::atoi(Attr(tag, "behavior").c_str());
		p.autoTrigger = Attr(tag, "autoTrigger") == "1" || Attr(tag, "autoTrigger") == "true";
		const std::string tr = Attr(tag, "triggerRange");
		if (!tr.empty())
			p.triggerRange = static_cast<float>(std::atof(tr.c_str()));
		p.tipName = Attr(tag, "tip-name");
		p.tipDescription = Attr(tag, "tip-description");
		p.info = Attr(tag, "info");
		p.copy = Attr(tag, "copy");
		p.copyMessage = Attr(tag, "copy-message");
		p.schedule = Attr(tag, "schedule");
		const std::string sd = Attr(tag, "schedule-duration");
		if (!sd.empty())
			p.scheduleDuration = static_cast<float>(std::atof(sd.c_str()));
		p.iconFile = Attr(tag, "iconFile");
		p.scriptOnce = Attr(tag, "script-once");
		if (p.scriptOnce.empty()) p.scriptOnce = Attr(tag, "scriptOnce");
		p.scriptTrigger = Attr(tag, "script-trigger");
		p.scriptFilter = Attr(tag, "script-filter");
		p.scriptTick = Attr(tag, "script-tick");
		p.scriptFocus = Attr(tag, "script-focus");
		p.hide = Attr(tag, "hide");
		p.show = Attr(tag, "show");
		if (p.mapId && !p.type.empty())
			gDraft.pois.push_back(std::move(p));
		pos = end + 2;
	}
	SetStatus("Loaded draft session (%zu POIs).", gDraft.pois.size());
	return true;
}
