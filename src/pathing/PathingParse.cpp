#include "PathingParse.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

#include "miniz/miniz.h"

namespace PathingParse
{
std::string ToLower(std::string s)
{
	for (char& c : s)
		if (c >= 'A' && c <= 'Z')
			c = static_cast<char>(c - 'A' + 'a');
	return s;
}

bool LooksLikeMapCompletion(const std::string& type, const std::string& path)
{
	const std::string t = ToLower(type);
	const std::string p = ToLower(path);
	if (t.rfind("legs.map.", 0) == 0 || t.find(".map.") != std::string::npos)
		return true;
	if (t.rfind("tt.mc.", 0) == 0 || t.find(".mc.") != std::string::npos)
		return true;
	/* Tekkit uses tw_guides.tw_mc.… (underscore, not .mc.) */
	if (t.find(".tw_mc.") != std::string::npos || t.find("tw_mc.") != std::string::npos)
		return true;
	if (t.find("mapcompletion") != std::string::npos)
		return true;
	if (p.find("map completion") != std::string::npos)
		return true;
	if (p.find("map_completion") != std::string::npos)
		return true;
	if (p.find("/tw_mc_") != std::string::npos || p.find("tw_mc_") != std::string::npos)
		return true;
	if (p.find("/tw/mc/") != std::string::npos)
		return true;
	if (p.find("barefoot") != std::string::npos && p.find("map") != std::string::npos)
		return true;
	return false;
}

void DecodeXmlEntities(std::string& text)
{
	auto replaceAll = [&](const char* from, const char* to) {
		const size_t fromLen = std::strlen(from);
		const size_t toLen = std::strlen(to);
		size_t pos = 0;
		while ((pos = text.find(from, pos)) != std::string::npos)
		{
			text.replace(pos, fromLen, to);
			pos += toLen;
		}
	};
	replaceAll("&#xA;", "\n");
	replaceAll("&#XA;", "\n");
	replaceAll("&#xa;", "\n");
	replaceAll("&#10;", "\n");
	replaceAll("&quot;", "\"");
	replaceAll("&apos;", "'");
	replaceAll("&lt;", "<");
	replaceAll("&gt;", ">");
	replaceAll("&amp;", "&");
}

uint32_t ParseColorAttr(const std::string& tag)
{
	size_t p = tag.find("color=\"");
	if (p == std::string::npos)
		p = tag.find("Color=\"");
	if (p == std::string::npos)
		return 0xFF00FFFF;
	p = tag.find('"', p) + 1;
	size_t e = tag.find('"', p);
	if (e == std::string::npos || e <= p)
		return 0xFF00FFFF;
	std::string hex = tag.substr(p, e - p);
	if (!hex.empty() && hex[0] == '#')
		hex.erase(0, 1);
	if (hex.size() != 6 && hex.size() != 8)
		return 0xFF00FFFF;
	uint32_t v = 0;
	for (char c : hex)
	{
		v <<= 4;
		if (c >= '0' && c <= '9')
			v |= static_cast<uint32_t>(c - '0');
		else if (c >= 'a' && c <= 'f')
			v |= static_cast<uint32_t>(c - 'a' + 10);
		else if (c >= 'A' && c <= 'F')
			v |= static_cast<uint32_t>(c - 'A' + 10);
	}
	if (hex.size() == 6)
		v |= 0xFF000000u;
	return v;
}

std::string Attr(const std::string& tag, const char* key)
{
	std::string k = std::string(key) + "=\"";
	size_t p = tag.find(k);
	if (p == std::string::npos)
	{
		/* case-insensitive key search */
		std::string low = ToLower(tag);
		std::string kl = ToLower(k);
		p = low.find(kl);
		if (p == std::string::npos)
			return {};
	}
	p = tag.find('"', p) + 1;
	size_t e = tag.find('"', p);
	if (e == std::string::npos)
		return {};
	return tag.substr(p, e - p);
}

bool ParseBoolValue(const std::string& value, bool fallback)
{
	if (value.empty())
		return fallback;
	const std::string low = ToLower(value);
	if (low == "0" || low == "false" || low == "no")
		return false;
	if (low == "1" || low == "true" || low == "yes")
		return true;
	return fallback;
}

void MergeStyle(MarkerStyle& dst, const MarkerStyle& src)
{
	if (src.hasIconFile) { dst.iconFile = src.iconFile; dst.hasIconFile = true; }
	if (src.hasTexture) { dst.texture = src.texture; dst.hasTexture = true; }
	if (src.hasMinimapVisible) { dst.minimapVisible = src.minimapVisible; dst.hasMinimapVisible = true; }
	if (src.hasMapVisible) { dst.mapVisible = src.mapVisible; dst.hasMapVisible = true; }
	if (src.hasInGameVisible) { dst.inGameVisible = src.inGameVisible; dst.hasInGameVisible = true; }
	if (src.hasMapDisplaySize) { dst.mapDisplaySize = src.mapDisplaySize; dst.hasMapDisplaySize = true; }
	if (src.hasMinSize) { dst.minSize = src.minSize; dst.hasMinSize = true; }
	if (src.hasMaxSize) { dst.maxSize = src.maxSize; dst.hasMaxSize = true; }
	if (src.hasIconSize) { dst.iconSize = src.iconSize; dst.hasIconSize = true; }
	if (src.hasHeightOffset) { dst.heightOffset = src.heightOffset; dst.hasHeightOffset = true; }
	if (src.hasFadeNear) { dst.fadeNear = src.fadeNear; dst.hasFadeNear = true; }
	if (src.hasFadeFar) { dst.fadeFar = src.fadeFar; dst.hasFadeFar = true; }
	if (src.hasAlpha) { dst.alpha = src.alpha; dst.hasAlpha = true; }
	if (src.hasTrailScale) { dst.trailScale = src.trailScale; dst.hasTrailScale = true; }
	if (src.hasColor) { dst.color = src.color; dst.hasColor = true; }
	if (src.hasBehavior) { dst.behavior = src.behavior; dst.hasBehavior = true; }
	if (src.hasAutoTrigger) { dst.autoTrigger = src.autoTrigger; dst.hasAutoTrigger = true; }
	if (src.hasTriggerRange) { dst.triggerRange = src.triggerRange; dst.hasTriggerRange = true; }
	if (src.hasResetLength) { dst.resetLength = src.resetLength; dst.hasResetLength = true; }
	if (src.hasInvertBehavior) { dst.invertBehavior = src.invertBehavior; dst.hasInvertBehavior = true; }
	if (src.hasHide) { dst.hide = src.hide; dst.hasHide = true; }
	if (src.hasShow) { dst.show = src.show; dst.hasShow = true; }
	if (src.hasTipName) { dst.tipName = src.tipName; dst.hasTipName = true; }
	if (src.hasTipDescription) { dst.tipDescription = src.tipDescription; dst.hasTipDescription = true; }
	if (src.hasInfo) { dst.info = src.info; dst.hasInfo = true; }
	if (src.hasCopy) { dst.copy = src.copy; dst.hasCopy = true; }
	if (src.hasCopyMessage) { dst.copyMessage = src.copyMessage; dst.hasCopyMessage = true; }
}

MarkerStyle ParseStyle(const std::string& tag)
{
	MarkerStyle out;
	auto compatible = [&](const char* key) {
		std::string value = Attr(tag, key);
		if (value.empty())
			value = Attr(tag, (std::string("bh-") + key).c_str());
		return value;
	};
	std::string 	value = compatible("iconFile");
	if (!value.empty())
	{
		DecodeXmlEntities(value);
		std::replace(value.begin(), value.end(), '\\', '/');
		out.iconFile = std::move(value);
		out.hasIconFile = true;
	}
	value = compatible("texture");
	if (!value.empty())
	{
		DecodeXmlEntities(value);
		std::replace(value.begin(), value.end(), '\\', '/');
		out.texture = std::move(value);
		out.hasTexture = true;
	}
	value = compatible("minimapVisibility");
	if (!value.empty())
	{
		out.minimapVisible = ParseBoolValue(value);
		out.hasMinimapVisible = true;
	}
	value = compatible("mapVisibility");
	if (!value.empty())
	{
		out.mapVisible = ParseBoolValue(value);
		out.hasMapVisible = true;
	}
	value = compatible("inGameVisibility");
	if (!value.empty())
	{
		out.inGameVisible = ParseBoolValue(value);
		out.hasInGameVisible = true;
	}
	value = compatible("mapDisplaySize");
	if (!value.empty())
	{
		out.mapDisplaySize = static_cast<float>(std::atof(value.c_str()));
		out.hasMapDisplaySize = std::isfinite(out.mapDisplaySize);
	}
	value = compatible("minSize");
	if (!value.empty())
	{
		out.minSize = static_cast<float>(std::atof(value.c_str()));
		out.hasMinSize = std::isfinite(out.minSize);
	}
	value = compatible("maxSize");
	if (!value.empty())
	{
		out.maxSize = static_cast<float>(std::atof(value.c_str()));
		out.hasMaxSize = std::isfinite(out.maxSize);
	}
	value = compatible("iconSize");
	if (!value.empty())
	{
		out.iconSize = static_cast<float>(std::atof(value.c_str()));
		out.hasIconSize = std::isfinite(out.iconSize);
	}
	value = compatible("heightOffset");
	if (!value.empty())
	{
		out.heightOffset = static_cast<float>(std::atof(value.c_str()));
		out.hasHeightOffset = std::isfinite(out.heightOffset);
	}
	value = compatible("fadeNear");
	if (!value.empty())
	{
		out.fadeNear = static_cast<float>(std::atof(value.c_str()));
		out.hasFadeNear = std::isfinite(out.fadeNear);
	}
	value = compatible("fadeFar");
	if (!value.empty())
	{
		out.fadeFar = static_cast<float>(std::atof(value.c_str()));
		out.hasFadeFar = std::isfinite(out.fadeFar);
	}
	value = compatible("alpha");
	if (!value.empty())
	{
		out.alpha = std::clamp(static_cast<float>(std::atof(value.c_str())), 0.f, 1.f);
		out.hasAlpha = true;
	}
	value = compatible("trailScale");
	if (!value.empty())
	{
		out.trailScale = static_cast<float>(std::atof(value.c_str()));
		out.hasTrailScale = std::isfinite(out.trailScale);
	}
	value = Attr(tag, "color");
	if (value.empty()) value = Attr(tag, "bh-color");
	if (value.empty()) value = Attr(tag, "tint");
	if (!value.empty())
	{
		const std::string synthetic = "color=\"" + value + "\"";
		out.color = ParseColorAttr(synthetic);
		out.hasColor = true;
	}

	value = compatible("behavior");
	if (!value.empty())
	{
		out.behavior = std::atoi(value.c_str());
		out.hasBehavior = true;
	}
	value = compatible("autoTrigger");
	if (value.empty()) value = compatible("AutoTrigger");
	if (!value.empty())
	{
		out.autoTrigger = ParseBoolValue(value, false);
		out.hasAutoTrigger = true;
	}
	value = compatible("triggerRange");
	if (value.empty()) value = compatible("TriggerRange");
	if (!value.empty())
	{
		out.triggerRange = static_cast<float>(std::atof(value.c_str()));
		out.hasTriggerRange = std::isfinite(out.triggerRange);
	}
	value = compatible("resetLength");
	if (value.empty()) value = compatible("ResetLength");
	if (!value.empty())
	{
		out.resetLength = static_cast<float>(std::atof(value.c_str()));
		out.hasResetLength = std::isfinite(out.resetLength);
	}
	value = compatible("invertBehavior");
	if (value.empty()) value = compatible("InvertBehavior");
	if (!value.empty())
	{
		out.invertBehavior = ParseBoolValue(value, false);
		out.hasInvertBehavior = true;
	}
	value = compatible("hide");
	if (!value.empty())
	{
		out.hide = std::move(value);
		out.hasHide = true;
	}
	value = compatible("show");
	if (!value.empty())
	{
		out.show = std::move(value);
		out.hasShow = true;
	}
	value = Attr(tag, "tip-name");
	if (value.empty()) value = Attr(tag, "tipName");
	if (!value.empty())
	{
		DecodeXmlEntities(value);
		out.tipName = std::move(value);
		out.hasTipName = true;
	}
	value = Attr(tag, "tip-description");
	if (value.empty()) value = Attr(tag, "tipDescription");
	if (!value.empty())
	{
		DecodeXmlEntities(value);
		out.tipDescription = std::move(value);
		out.hasTipDescription = true;
	}
	value = compatible("info");
	if (!value.empty())
	{
		DecodeXmlEntities(value);
		out.info = std::move(value);
		out.hasInfo = true;
	}
	value = compatible("copy");
	if (!value.empty())
	{
		DecodeXmlEntities(value);
		out.copy = std::move(value);
		out.hasCopy = true;
	}
	value = Attr(tag, "copy-message");
	if (value.empty()) value = Attr(tag, "copyMessage");
	if (!value.empty())
	{
		DecodeXmlEntities(value);
		out.copyMessage = std::move(value);
		out.hasCopyMessage = true;
	}
	return out;
}

MarkerStyle ResolveStyle(
	const std::string& type,
	const MarkerStyle& own,
	const std::unordered_map<std::string, MarkerStyle>& categories)
{
	MarkerStyle out;
	const std::string typeLow = ToLower(type);
	size_t pos = 0;
	while (pos < typeLow.size())
	{
		const size_t dot = typeLow.find('.', pos);
		const size_t end = (dot == std::string::npos) ? typeLow.size() : dot;
		const std::string prefix = typeLow.substr(0, end);
		auto it = categories.find(prefix);
		if (it != categories.end())
			MergeStyle(out, it->second);
		if (dot == std::string::npos)
			break;
		pos = dot + 1;
	}
	MergeStyle(out, own);
	return out;
}

/* ---- ZIP via miniz (TacO .taco is a zip) ---- */
bool ReadFileW(const std::wstring& path, std::vector<uint8_t>& out, size_t maxBytes)
{
	out.clear();
	FILE* f = _wfopen(path.c_str(), L"rb");
	if (!f)
		return false;
	std::fseek(f, 0, SEEK_END);
	const long sz = std::ftell(f);
	std::fseek(f, 0, SEEK_SET);
	if (sz <= 0 || static_cast<size_t>(sz) > maxBytes)
	{
		std::fclose(f);
		return false;
	}
	out.resize(static_cast<size_t>(sz));
	const bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
	std::fclose(f);
	if (!ok)
		out.clear();
	return ok;
}

/* Fast lookup via the zip central directory (binary search) instead of a
   linear scan per trail — critical to avoid a startup freeze with big packs. */
int ZipLocate(mz_zip_archive& zip, const std::string& entryName)
{
	std::string want = entryName;
	std::replace(want.begin(), want.end(), '\\', '/');
	int idx = mz_zip_reader_locate_file(&zip, want.c_str(), nullptr, 0);
	if (idx >= 0)
		return idx;
	/* Some packs store backslash separators — try that form too. */
	std::string alt = entryName;
	std::replace(alt.begin(), alt.end(), '/', '\\');
	return mz_zip_reader_locate_file(&zip, alt.c_str(), nullptr, 0);
}

bool ZipExtractIndex(mz_zip_archive& zip, int fileIndex,
	std::vector<uint8_t>& out, size_t maxOut)
{
	out.clear();
	if (fileIndex < 0)
		return false;
	mz_zip_archive_file_stat st{};
	if (!mz_zip_reader_file_stat(&zip, static_cast<mz_uint>(fileIndex), &st) ||
		st.m_is_directory)
		return false;
	if (st.m_uncomp_size == 0 || st.m_uncomp_size > maxOut)
		return false;
	size_t sz = 0;
	void* mem = mz_zip_reader_extract_to_heap(&zip, static_cast<mz_uint>(fileIndex), &sz, 0);
	if (!mem || sz == 0)
		return false;
	out.assign(static_cast<uint8_t*>(mem), static_cast<uint8_t*>(mem) + sz);
	mz_free(mem);
	return true;
}

bool ZipExtractEntry(mz_zip_archive& zip, const std::string& entryName,
	std::vector<uint8_t>& out, size_t maxOut)
{
	return ZipExtractIndex(zip, ZipLocate(zip, entryName), out, maxOut);
}

bool ZipReadEntry(const std::wstring& zipPath, const std::string& entryName,
	std::vector<uint8_t>& out, size_t maxOut)
{
	out.clear();
	std::vector<uint8_t> file;
	if (!ReadFileW(zipPath, file, kMaxZipBytes))
		return false;

	mz_zip_archive zip{};
	if (!mz_zip_reader_init_mem(&zip, file.data(), file.size(), 0))
		return false;
	const bool ok = ZipExtractEntry(zip, entryName, out, maxOut);
	mz_zip_reader_end(&zip);
	return ok;
}

/* Read only the .trl header (version + mapId = 8 bytes) via a streaming
   iterator so we never decompress the whole trail just to learn its map. */
uint32_t PeekTrlMapId(mz_zip_archive& zip, int fileIndex)
{
	if (fileIndex < 0)
		return 0;
	mz_zip_reader_extract_iter_state* it =
		mz_zip_reader_extract_iter_new(&zip, static_cast<mz_uint>(fileIndex), 0);
	if (!it)
		return 0;
	uint8_t hdr[8]{};
	const size_t got = mz_zip_reader_extract_iter_read(it, hdr, sizeof(hdr));
	mz_zip_reader_extract_iter_free(it);
	if (got < sizeof(hdr))
		return 0;
	uint32_t mid = 0;
	std::memcpy(&mid, hdr + 4, 4);
	if (mid == 0 || mid > 100000)
		return 0;
	return mid;
}

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

bool ParseTrl(const std::vector<uint8_t>& data, uint32_t& mapId,
	std::vector<PathingTrails::WorldPoint>& world)
{
	world.clear();
	if (data.size() < 20)
		return false;
	/* version (u32) + mapId (u32) + N * float3 (x,y,z) — Y up, horizontal = x,z.
	   TacO/Blish/Taimi: a (0,0,0) point ends a trail *section*. Connecting across
	   those breaks draws compass spaghetti (hub → every next segment). */
	uint32_t ver = 0, mid = 0;
	std::memcpy(&ver, data.data(), 4);
	std::memcpy(&mid, data.data() + 4, 4);
	(void)ver;
	mapId = mid;
	if (mapId == 0 || mapId > 100000)
		return false;

	size_t off = 8;
	size_t rem = data.size() - 8;
	rem -= rem % 12;
	const size_t count = rem / 12;
	if (count < 2)
		return false;

	auto isBreak = [](float x, float y, float z) -> bool
	{
		return x == 0.f && y == 0.f && z == 0.f;
	};
	auto okPoint = [](float x, float y, float z) -> bool
	{
		return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) &&
			std::fabs(x) < 1.0e6f && std::fabs(y) < 1.0e6f && std::fabs(z) < 1.0e6f &&
			!(x == 0.f && y == 0.f && z == 0.f);
	};

	/* Collect every section first — Lady HP trails are multi-section and used to
	   drop everything after a flat 512-point budget (paths stopped mid-map). */
	std::vector<std::vector<PathingTrails::WorldPoint>> sections;
	sections.reserve(64);
	std::vector<PathingTrails::WorldPoint> section;
	section.reserve(64);
	size_t totalPts = 0;

	auto flushSection = [&]()
	{
		if (section.size() < 2)
		{
			section.clear();
			return;
		}
		totalPts += section.size();
		sections.push_back(std::move(section));
		section.clear();
		section.reserve(64);
	};

	for (size_t i = 0; i < count; ++i)
	{
		float x = 0.f, y = 0.f, z = 0.f;
		std::memcpy(&x, data.data() + off + i * 12, 4);
		std::memcpy(&y, data.data() + off + i * 12 + 4, 4);
		std::memcpy(&z, data.data() + off + i * 12 + 8, 4);
		if (isBreak(x, y, z))
		{
			flushSection();
			continue;
		}
		if (!okPoint(x, y, z))
			continue;
		section.push_back({x, y, z});
	}
	flushSection();
	if (sections.empty())
		return false;

	auto appendDecimated = [&](const std::vector<PathingTrails::WorldPoint>& src, size_t budget)
	{
		if (src.size() < 2 || budget < 2)
			return;
		if (!world.empty())
			world.push_back({NAN, NAN, NAN});
		if (src.size() <= budget)
		{
			world.insert(world.end(), src.begin(), src.end());
			return;
		}
		for (size_t k = 0; k < budget; ++k)
		{
			const size_t i = (k * (src.size() - 1)) / (budget - 1);
			world.push_back(src[i]);
		}
	};

	/* Leave room for NaN section breaks in the point budget. */
	const size_t breakBudget = sections.size() > 0 ? sections.size() - 1 : 0;
	size_t pointBudget = kMaxPointsPerTrail > breakBudget
		? (kMaxPointsPerTrail - breakBudget) : 2;
	world.reserve(std::min(totalPts + breakBudget, kMaxPointsPerTrail));

	if (totalPts <= pointBudget)
	{
		for (const auto& sec : sections)
			appendDecimated(sec, sec.size());
	}
	else
	{
		/* Spread budget across sections so later HP segments are not dropped. */
		size_t assigned = 0;
		for (size_t s = 0; s < sections.size(); ++s)
		{
			const size_t leftSecs = sections.size() - s;
			const size_t leftBudget = pointBudget > assigned ? (pointBudget - assigned) : 0;
			size_t share = std::max<size_t>(2, (sections[s].size() * pointBudget) / totalPts);
			if (share > leftBudget)
				share = leftBudget;
			/* Keep enough for remaining sections (2 pts each). */
			const size_t needRest = (leftSecs - 1) * 2;
			if (leftBudget > needRest && share > leftBudget - needRest)
				share = leftBudget - needRest;
			if (share < 2)
				share = leftBudget >= 2 ? 2 : leftBudget;
			appendDecimated(sections[s], share);
			assigned += share;
		}
	}
	return world.size() >= 2;
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
