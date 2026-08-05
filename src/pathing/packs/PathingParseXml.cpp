#include "PathingParse.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace PathingParse
{
void IndexXml(const std::wstring& packPath, const std::string& xml,
	std::vector<IndexedTrail>& out)
{
	size_t pos = 0;
	while (pos < xml.size() && out.size() < 30000)
	{
		size_t t = xml.find("<Trail", pos);
		if (t == std::string::npos)
			t = xml.find("<trail", pos);
		if (t == std::string::npos)
			break;
		size_t end = xml.find('>', t);
		if (end == std::string::npos)
			break;
		const std::string tag = xml.substr(t, end - t + 1);
		pos = end + 1;

		std::string data = Attr(tag, "trailData");
		if (data.empty())
			data = Attr(tag, "TrailData");
		if (data.empty())
			continue;
		std::replace(data.begin(), data.end(), '\\', '/');
		while (!data.empty() && (data[0] == '.' || data[0] == '/'))
		{
			if (data.rfind("./", 0) == 0)
				data.erase(0, 2);
			else if (data[0] == '/')
				data.erase(0, 1);
			else
				break;
		}

		IndexedTrail it;
		it.packPath = packPath;
		it.entryName = data;
		it.type = Attr(tag, "type");
		it.color = 0xFFFFFFFFu;
		it.mapCompletion = LooksLikeMapCompletion(it.type, it.entryName);
		it.style = ParseStyle(tag);
		out.push_back(std::move(it));
	}
}

void CollectCategoryMapIds(const std::string& xml,
	std::unordered_map<std::string, uint32_t>& categoryMapIds)
{
	/* Same nesting walk as ParseMarkerMenuXml — MapID on a category applies to
	   descendant POIs that omit MapID (Hero's Fractal Dailies, Twin Largos, …). */
	struct Frame
	{
		std::string path;
		uint32_t mapId = 0;
	};
	std::vector<Frame> stack;
	size_t pos = 0;
	while (pos < xml.size())
	{
		size_t t = xml.find('<', pos);
		if (t == std::string::npos)
			break;
		if (t + 1 < xml.size() && xml[t + 1] == '/')
		{
			size_t end = xml.find('>', t);
			if (end == std::string::npos)
				break;
			const std::string close = ToLower(xml.substr(t + 2, end - (t + 2)));
			if (close.find("markercategory") == 0 && !stack.empty())
				stack.pop_back();
			pos = end + 1;
			continue;
		}
		const bool isCat =
			xml.compare(t, 15, "<MarkerCategory") == 0 ||
			xml.compare(t, 15, "<markercategory") == 0;
		if (!isCat)
		{
			pos = t + 1;
			continue;
		}
		size_t end = xml.find('>', t);
		if (end == std::string::npos)
			break;
		const std::string tag = xml.substr(t, end - t + 1);
		pos = end + 1;

		std::string name = Attr(tag, "name");
		if (name.empty())
			name = Attr(tag, "Name");
		if (name.empty())
			continue;
		std::string path = stack.empty() ? name : (stack.back().path + "." + name);
		uint32_t mapId = 0;
		std::string mapStr = Attr(tag, "MapID");
		if (mapStr.empty()) mapStr = Attr(tag, "mapid");
		if (mapStr.empty()) mapStr = Attr(tag, "MapId");
		if (!mapStr.empty())
			mapId = static_cast<uint32_t>(std::strtoul(mapStr.c_str(), nullptr, 10));
		if (mapId == 0 && !stack.empty())
			mapId = stack.back().mapId;
		if (mapId != 0)
			categoryMapIds[ToLower(path)] = mapId;

		const bool selfClose = tag.size() >= 2 && tag[tag.size() - 2] == '/';
		if (!selfClose)
			stack.push_back({path, mapId});
	}
}

void IndexPoisXml(const std::wstring& packPath, const std::string& xml,
	std::vector<IndexedPoi>& out,
	const std::unordered_map<std::string, uint32_t>& categoryMapIds)
{
	auto resolveMapId = [&](const std::string& type, uint32_t own) -> uint32_t {
		if (own != 0)
			return own;
		if (type.empty() || categoryMapIds.empty())
			return 0;
		std::string low = ToLower(type);
		while (!low.empty())
		{
			auto it = categoryMapIds.find(low);
			if (it != categoryMapIds.end() && it->second != 0)
				return it->second;
			const size_t dot = low.rfind('.');
			if (dot == std::string::npos)
				break;
			low.resize(dot);
		}
		return 0;
	};

	size_t pos = 0;
	while (pos < xml.size() && out.size() < 80000)
	{
		size_t t = xml.find("<POI", pos);
		if (t == std::string::npos)
			t = xml.find("<poi", pos);
		if (t == std::string::npos)
			break;
		/* Require word boundary so we don't match unrelated tags. */
		if (t + 4 < xml.size())
		{
			const char c = xml[t + 4];
			if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '/' && c != '>')
			{
				pos = t + 4;
				continue;
			}
		}
		size_t end = xml.find('>', t);
		if (end == std::string::npos)
			break;
		const std::string tag = xml.substr(t, end - t + 1);
		pos = end + 1;

		std::string type = Attr(tag, "type");
		if (type.empty())
			type = Attr(tag, "Type");
		if (type.empty())
			continue;

		std::string mapStr = Attr(tag, "MapID");
		if (mapStr.empty())
			mapStr = Attr(tag, "mapid");
		if (mapStr.empty())
			mapStr = Attr(tag, "MapId");
		uint32_t mapId = static_cast<uint32_t>(std::strtoul(mapStr.c_str(), nullptr, 10));
		mapId = resolveMapId(type, mapId);
		if (mapId == 0)
			continue;

		std::string xs = Attr(tag, "xpos");
		if (xs.empty())
			xs = Attr(tag, "XPos");
		std::string ys = Attr(tag, "ypos");
		if (ys.empty())
			ys = Attr(tag, "YPos");
		std::string zs = Attr(tag, "zpos");
		if (zs.empty())
			zs = Attr(tag, "ZPos");
		if (xs.empty() || zs.empty())
			continue;

		IndexedPoi poi;
		poi.packPath = packPath;
		poi.type = std::move(type);
		poi.mapId = mapId;
		poi.wx = static_cast<float>(std::atof(xs.c_str()));
		poi.wy = ys.empty() ? 0.f : static_cast<float>(std::atof(ys.c_str()));
		poi.wz = static_cast<float>(std::atof(zs.c_str()));
		poi.style = ParseStyle(tag);
		poi.guid = Attr(tag, "GUID");
		if (poi.guid.empty())
			poi.guid = Attr(tag, "guid");
		if (!std::isfinite(poi.wx) || !std::isfinite(poi.wz))
			continue;
		out.push_back(std::move(poi));
	}
}

/* Parse Tekkit MarkerCategory menu (DisplayName + order) from overlay XML. */
void ParseMarkerMenuXml(
	const std::string& xml,
	std::vector<PathingTrails::Category>& roots,
	std::unordered_map<std::string, MarkerStyle>& styles)
{
	struct Frame
	{
		PathingTrails::Category* node;
		std::string path;
	};
	std::vector<Frame> stack;
	size_t pos = 0;
	while (pos < xml.size())
	{
		size_t t = xml.find('<', pos);
		if (t == std::string::npos)
			break;
		if (t + 1 < xml.size() && xml[t + 1] == '/')
		{
			size_t end = xml.find('>', t);
			if (end == std::string::npos)
				break;
			const std::string close = ToLower(xml.substr(t + 2, end - (t + 2)));
			if (close.find("markercategory") == 0 && !stack.empty())
				stack.pop_back();
			pos = end + 1;
			continue;
		}

		const bool isCat =
			xml.compare(t, 15, "<MarkerCategory") == 0 ||
			xml.compare(t, 15, "<markercategory") == 0;
		if (!isCat)
		{
			pos = t + 1;
			continue;
		}
		size_t end = xml.find('>', t);
		if (end == std::string::npos)
			break;
		const std::string tag = xml.substr(t, end - t + 1);
		pos = end + 1;

		std::string name = Attr(tag, "name");
		if (name.empty())
			name = Attr(tag, "Name");
		if (name.empty())
			continue;
		std::string display = Attr(tag, "DisplayName");
		if (display.empty())
			display = Attr(tag, "displayname");
		if (display.empty())
			display = name;
		const std::string sepAttr = ToLower(Attr(tag, "IsSeparator"));
		const bool sep = (sepAttr == "1" || sepAttr == "true") ||
			ToLower(name).find("separator") != std::string::npos;

		std::string path = stack.empty() ? name : (stack.back().path + "." + name);
		MarkerStyle style = ParseStyle(tag);
		/* Only promote DisplayName → tip for mount/shortcut leaves — copying it
		   for every category (Three/Four/…) flooded world labels + tip UI. */
		if (!style.hasTipName && !display.empty() && !sep)
		{
			const std::string pathLow = ToLower(path);
			const bool mountLeaf =
				pathLow.find(".bfs.") != std::string::npos ||
				pathLow.find("images/mounts/") != std::string::npos ||
				pathLow.find(".mount.") != std::string::npos ||
				pathLow.find(".mounts.") != std::string::npos ||
				(style.hasIconFile && ToLower(style.iconFile).find("images/mounts/") != std::string::npos);
			if (mountLeaf)
			{
				style.tipName = display;
				style.hasTipName = true;
			}
		}
		const std::string stylePath = ToLower(path);
		auto styleIt = styles.find(stylePath);
		if (styleIt == styles.end())
			styles.emplace(stylePath, style);
		else
			MergeStyle(styleIt->second, style);
		PathingTrails::Category neu;
		neu.path = path;
		neu.label = display;
		neu.separator = sep;
		if (style.hasTipDescription)
			neu.tip = style.tipDescription;
		std::string hidden = Attr(tag, "IsHidden");
		if (hidden.empty()) hidden = Attr(tag, "bh-IsHidden");
		neu.hidden = ParseBoolValue(hidden, false);
		neu.trails = 0;
		neu.enabled = false;

		std::vector<PathingTrails::Category>* dest =
			stack.empty() ? &roots : &stack.back().node->children;
		dest->push_back(std::move(neu));
		PathingTrails::Category* added = &dest->back();

		const bool selfClose = tag.size() >= 2 && tag[tag.size() - 2] == '/';
		if (!selfClose)
			stack.push_back({added, path});
	}
}

} // namespace PathingParse
