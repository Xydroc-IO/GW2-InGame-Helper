#include "PathingParse.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>

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
	if (src.hasSchedule) { dst.schedule = src.schedule; dst.hasSchedule = true; }
	if (src.hasScheduleDuration)
	{
		dst.scheduleDuration = src.scheduleDuration;
		dst.hasScheduleDuration = true;
	}
	if (src.hasScriptOnce) { dst.scriptOnce = src.scriptOnce; dst.hasScriptOnce = true; }
	if (src.hasScriptTrigger) { dst.scriptTrigger = src.scriptTrigger; dst.hasScriptTrigger = true; }
	if (src.hasScriptFilter) { dst.scriptFilter = src.scriptFilter; dst.hasScriptFilter = true; }
	if (src.hasScriptTick) { dst.scriptTick = src.scriptTick; dst.hasScriptTick = true; }
	if (src.hasScriptFocus) { dst.scriptFocus = src.scriptFocus; dst.hasScriptFocus = true; }
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
	value = compatible("schedule");
	if (!value.empty())
	{
		out.schedule = std::move(value);
		out.hasSchedule = true;
	}
	value = compatible("schedule-duration");
	if (value.empty()) value = compatible("scheduleDuration");
	if (!value.empty())
	{
		out.scheduleDuration = static_cast<float>(std::atof(value.c_str()));
		out.hasScheduleDuration = std::isfinite(out.scheduleDuration);
	}
	value = compatible("script-once");
	if (value.empty()) value = compatible("scriptOnce");
	if (!value.empty()) { out.scriptOnce = std::move(value); out.hasScriptOnce = true; }
	value = compatible("script-trigger");
	if (value.empty()) value = compatible("scriptTrigger");
	if (!value.empty()) { out.scriptTrigger = std::move(value); out.hasScriptTrigger = true; }
	value = compatible("script-filter");
	if (value.empty()) value = compatible("scriptFilter");
	if (!value.empty()) { out.scriptFilter = std::move(value); out.hasScriptFilter = true; }
	value = compatible("script-tick");
	if (value.empty()) value = compatible("scriptTick");
	if (!value.empty()) { out.scriptTick = std::move(value); out.hasScriptTick = true; }
	value = compatible("script-focus");
	if (value.empty()) value = compatible("scriptFocus");
	if (!value.empty()) { out.scriptFocus = std::move(value); out.hasScriptFocus = true; }
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

} // namespace PathingParse
