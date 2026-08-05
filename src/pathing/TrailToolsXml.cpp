#include "TrailToolsXml.h"

#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>

#include <windows.h>

namespace
{
	std::string EscAttr(const std::string& s)
	{
		std::string o;
		o.reserve(s.size() + 8);
		for (char c : s)
		{
			switch (c)
			{
			case '&': o += "&amp;"; break;
			case '"': o += "&quot;"; break;
			case '<': o += "&lt;"; break;
			case '>': o += "&gt;"; break;
			default: o.push_back(c); break;
			}
		}
		return o;
	}

	void EmitCategory(std::ostringstream& os, const TrailToolsDetail::CategoryNode& n, int indent)
	{
		std::string pad(static_cast<size_t>(indent), '\t');
		os << pad << "<MarkerCategory name=\"" << EscAttr(n.name)
			<< "\" DisplayName=\"" << EscAttr(n.displayName) << "\"";
		if (!n.iconFile.empty())
			os << " iconFile=\"" << EscAttr(n.iconFile) << "\"";
		if (!n.texture.empty())
			os << " texture=\"" << EscAttr(n.texture) << "\"";
		if (n.fadeNear >= 0.f)
			os << " fadeNear=\"" << n.fadeNear << "\"";
		if (n.fadeFar >= 0.f)
			os << " fadeFar=\"" << n.fadeFar << "\"";
		if (n.trailScale > 0.f && std::fabs(n.trailScale - 1.f) > 0.001f)
			os << " trailScale=\"" << n.trailScale << "\"";
		if (n.iconSize > 0.f && std::fabs(n.iconSize - 1.f) > 0.001f)
			os << " iconSize=\"" << n.iconSize << "\"";
		if (n.alpha > 0.f && n.alpha < 0.999f)
			os << " alpha=\"" << n.alpha << "\"";
		if (n.color != 0)
		{
			char hex[16]{};
			std::snprintf(hex, sizeof(hex), "%08X", n.color);
			os << " color=\"" << hex << "\"";
		}
		if (!n.schedule.empty())
		{
			os << " schedule=\"" << EscAttr(n.schedule) << "\"";
			if (n.scheduleDuration > 0.f)
				os << " schedule-duration=\"" << n.scheduleDuration << "\"";
		}
		if (n.children.empty())
		{
			os << "/>\n";
			return;
		}
		os << ">\n";
		for (const auto& ch : n.children)
			EmitCategory(os, ch, indent + 1);
		os << pad << "</MarkerCategory>\n";
	}

	void EmitPoi(std::ostringstream& os, const TrailToolsDetail::DraftPoi& p, int indentTabs)
	{
		std::string pad(static_cast<size_t>(indentTabs), '\t');
		os << pad << "<POI MapID=\"" << p.mapId
			<< "\" xpos=\"" << p.x << "\" ypos=\"" << p.y << "\" zpos=\"" << p.z
			<< "\" type=\"" << EscAttr(p.type) << "\" GUID=\"" << EscAttr(p.guid) << "\"";
		if (p.behavior != 0)
			os << " behavior=\"" << p.behavior << "\"";
		if (p.autoTrigger)
			os << " autoTrigger=\"1\"";
		if (p.triggerRange > 0.f && std::fabs(p.triggerRange - 2.f) > 0.001f)
			os << " triggerRange=\"" << p.triggerRange << "\"";
		if (p.resetLength > 0.f)
			os << " resetLength=\"" << p.resetLength << "\"";
		if (p.invertBehavior)
			os << " invertBehavior=\"1\"";
		if (p.fadeNear >= 0.f)
			os << " fadeNear=\"" << p.fadeNear << "\"";
		if (p.fadeFar >= 0.f)
			os << " fadeFar=\"" << p.fadeFar << "\"";
		if (p.alpha > 0.f && p.alpha < 0.999f)
			os << " alpha=\"" << p.alpha << "\"";
		if (p.iconSize > 0.f && std::fabs(p.iconSize - 1.f) > 0.001f)
			os << " iconSize=\"" << p.iconSize << "\"";
		if (std::fabs(p.heightOffset - 1.5f) > 0.001f)
			os << " heightOffset=\"" << p.heightOffset << "\"";
		if (!p.minimapVisible)
			os << " miniMapVisibility=\"0\"";
		if (!p.inGameVisible)
			os << " inGameVisibility=\"0\"";
		if (!p.tipName.empty())
			os << " tip-name=\"" << EscAttr(p.tipName) << "\"";
		if (!p.tipDescription.empty())
			os << " tip-description=\"" << EscAttr(p.tipDescription) << "\"";
		if (!p.info.empty())
			os << " info=\"" << EscAttr(p.info) << "\"";
		if (!p.copy.empty())
			os << " copy=\"" << EscAttr(p.copy) << "\"";
		if (!p.copyMessage.empty())
			os << " copy-message=\"" << EscAttr(p.copyMessage) << "\"";
		if (!p.hide.empty())
			os << " hide=\"" << EscAttr(p.hide) << "\"";
		if (!p.show.empty())
			os << " show=\"" << EscAttr(p.show) << "\"";
		if (!p.schedule.empty())
		{
			os << " schedule=\"" << EscAttr(p.schedule) << "\"";
			if (p.scheduleDuration > 0.f)
				os << " schedule-duration=\"" << p.scheduleDuration << "\"";
		}
		if (!p.iconFile.empty())
			os << " iconFile=\"" << EscAttr(p.iconFile) << "\"";
		if (!p.scriptOnce.empty())
			os << " script-once=\"" << EscAttr(p.scriptOnce) << "\"";
		if (!p.scriptTrigger.empty())
			os << " script-trigger=\"" << EscAttr(p.scriptTrigger) << "\"";
		if (!p.scriptFilter.empty())
			os << " script-filter=\"" << EscAttr(p.scriptFilter) << "\"";
		if (!p.scriptTick.empty())
			os << " script-tick=\"" << EscAttr(p.scriptTick) << "\"";
		if (!p.scriptFocus.empty())
			os << " script-focus=\"" << EscAttr(p.scriptFocus) << "\"";
		os << "/>\n";
	}

	void EmitTrailLines(std::ostringstream& os, const TrailToolsDetail::DraftPack& pack, int indentTabs)
	{
		std::string pad(static_cast<size_t>(indentTabs), '\t');
		for (const auto& t : pack.trails)
		{
			if (t.fileRel.empty() || t.type.empty())
				continue;
			os << pad << "<Trail type=\"" << EscAttr(t.type)
				<< "\" trailData=\"" << EscAttr(t.fileRel) << "\"/>\n";
		}
		if (pack.active.points.size() >= 2 && !pack.active.fileRel.empty() &&
			!pack.active.type.empty())
		{
			bool listed = false;
			for (const auto& t : pack.trails)
			{
				if (t.fileRel == pack.active.fileRel)
				{
					listed = true;
					break;
				}
			}
			if (!listed)
			{
				os << pad << "<Trail type=\"" << EscAttr(pack.active.type)
					<< "\" trailData=\"" << EscAttr(pack.active.fileRel) << "\"/>\n";
			}
		}
	}

	void EmitPoisBlock(std::ostringstream& os, const TrailToolsDetail::DraftPack& pack)
	{
		os << "\t<POIs>\n";
		for (const auto& p : pack.pois)
			EmitPoi(os, p, 2);
		EmitTrailLines(os, pack, 2);
		os << "\t</POIs>\n";
	}

	std::wstring PackRootXml(const char* packName, const wchar_t* suffix)
	{
		std::wstring p = TrailToolsDetail::PackDir();
		p.push_back(L'\\');
		for (const char* c = packName; *c; ++c)
			p.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*c)));
		p += suffix;
		return p;
	}
}

std::string TrailToolsXml::EmitTrailElement(const TrailToolsDetail::DraftTrail& trail)
{
	if (trail.fileRel.empty() || trail.type.empty())
		return {};
	std::ostringstream os;
	os << "<Trail type=\"" << EscAttr(trail.type)
		<< "\" trailData=\"" << EscAttr(trail.fileRel) << "\"/>";
	return os.str();
}

std::string TrailToolsXml::EmitPoiElement(const TrailToolsDetail::DraftPoi& poi)
{
	std::ostringstream os;
	EmitPoi(os, poi, 0);
	std::string s = os.str();
	while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
		s.pop_back();
	return s;
}

std::string TrailToolsXml::EmitMenuOverlay(const TrailToolsDetail::DraftPack& pack)
{
	std::ostringstream os;
	os << "<!-- Generated by GW2-InGame-Helper Trail Tools (menu) -->\n";
	os << "<OverlayData>\n";
	EmitCategory(os, pack.root, 1);
	os << "</OverlayData>\n";
	return os.str();
}

std::string TrailToolsXml::EmitDataOverlay(const TrailToolsDetail::DraftPack& pack)
{
	std::ostringstream os;
	os << "<!-- Generated by GW2-InGame-Helper Trail Tools (data) -->\n";
	os << "<OverlayData>\n";
	EmitPoisBlock(os, pack);
	os << "</OverlayData>\n";
	return os.str();
}

std::string TrailToolsXml::EmitOverlayData(const TrailToolsDetail::DraftPack& pack)
{
	std::ostringstream os;
	os << "<!-- Generated by GW2-InGame-Helper Trail Tools -->\n";
	os << "<OverlayData>\n";
	EmitCategory(os, pack.root, 1);
	EmitPoisBlock(os, pack);
	os << "</OverlayData>\n";
	return os.str();
}

bool TrailToolsXml::WriteUtf8File(const std::wstring& path, const std::string& utf8)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	DWORD written = 0;
	const BOOL ok = WriteFile(h, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
	CloseHandle(h);
	return ok && written == utf8.size();
}

bool TrailToolsXml::WriteOverlayFile(const std::wstring& path, const TrailToolsDetail::DraftPack& pack)
{
	return WriteUtf8File(path, EmitOverlayData(pack));
}

bool TrailToolsXml::WritePackXmlLayout(const TrailToolsDetail::DraftPack& pack)
{
	const std::wstring combined = PackRootXml(pack.packName, L".xml");
	const std::wstring menu = PackRootXml(pack.packName, L"_Menu.xml");
	const std::wstring data = PackRootXml(pack.packName, L"_Data.xml");

	if (pack.xmlLayout == 1)
	{
		if (!WriteUtf8File(menu, EmitMenuOverlay(pack)))
			return false;
		if (!WriteUtf8File(data, EmitDataOverlay(pack)))
			return false;
		DeleteFileW(combined.c_str());
		return true;
	}

	if (!WriteUtf8File(combined, EmitOverlayData(pack)))
		return false;
	DeleteFileW(menu.c_str());
	DeleteFileW(data.c_str());
	return true;
}
