#include "WaypointsDataInternal.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace WaypointsDataDetail
{
size_t JsonObjectEnd(const std::string& json, size_t openBrace)
{
	if (openBrace >= json.size() || json[openBrace] != '{')
		return std::string::npos;
	int depth = 0;
	bool inStr = false, esc = false;
	for (size_t i = openBrace; i < json.size(); ++i)
	{
		char c = json[i];
		if (inStr)
		{
			if (esc) esc = false;
			else if (c == '\\') esc = true;
			else if (c == '"') inStr = false;
			continue;
		}
		if (c == '"') inStr = true;
		else if (c == '{') ++depth;
		else if (c == '}')
		{
			--depth;
			if (depth == 0) return i;
		}
	}
	return std::string::npos;
}

std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from)
{
	std::string pat = "\"";
	pat += key;
	pat += "\"";
	size_t k = json.find(pat, from);
	if (k == std::string::npos) return {};
	k = json.find(':', k + pat.size());
	if (k == std::string::npos) return {};
	++k;
	while (k < json.size() && (json[k] == ' ' || json[k] == '\t')) ++k;
	if (k >= json.size() || json[k] != '"') return {};
	++k;
	std::string out;
	while (k < json.size())
	{
		char c = json[k++];
		if (c == '\\' && k < json.size()) { out.push_back(json[k++]); continue; }
		if (c == '"') break;
		out.push_back(c);
	}
	return out;
}

bool JsonCoordAfterKey(const std::string& json, const char* key, size_t from,
	float& outX, float& outY)
{
	std::string pat = "\"";
	pat += key;
	pat += "\"";
	size_t k = json.find(pat, from);
	if (k == std::string::npos) return false;
	k = json.find(':', k + pat.size());
	if (k == std::string::npos) return false;
	++k;
	while (k < json.size() && (json[k] == ' ' || json[k] == '\t')) ++k;
	if (k >= json.size() || json[k] != '[') return false;
	++k;
	char* end1 = nullptr;
	const float x = static_cast<float>(std::strtod(json.c_str() + k, &end1));
	if (!end1 || end1 == json.c_str() + k) return false;
	k = static_cast<size_t>(end1 - json.c_str());
	while (k < json.size() && (json[k] == ' ' || json[k] == '\t' || json[k] == ',')) ++k;
	char* end2 = nullptr;
	const float y = static_cast<float>(std::strtod(json.c_str() + k, &end2));
	if (!end2 || end2 == json.c_str() + k) return false;
	outX = x;
	outY = y;
	return true;
}

long long JsonIntAfterKey(const std::string& json, const char* key, size_t from)
{
	std::string pat = "\"";
	pat += key;
	pat += "\"";
	size_t k = json.find(pat, from);
	if (k == std::string::npos) return -1;
	k = json.find(':', k + pat.size());
	if (k == std::string::npos) return -1;
	++k;
	while (k < json.size() && (json[k] == ' ' || json[k] == '\t')) ++k;
	bool neg = false;
	if (k < json.size() && json[k] == '-') { neg = true; ++k; }
	long long v = 0;
	bool any = false;
	while (k < json.size() && json[k] >= '0' && json[k] <= '9')
	{
		any = true;
		v = v * 10 + (json[k] - '0');
		++k;
	}
	if (!any) return -1;
	return neg ? -v : v;
}

void ParseFloorJson(const std::string& body, std::vector<WaypointsData::Poi>& pois)
{
	/* Each map embeds "points_of_interest":{…}. Use that anchor + nearby id/name. */
	size_t p = 0;
	while (p < body.size())
	{
		size_t poisKey = body.find("\"points_of_interest\"", p);
		if (poisKey == std::string::npos) break;
		size_t poisObj = body.find('{', poisKey);
		if (poisObj == std::string::npos) break;
		const size_t poisEnd = JsonObjectEnd(body, poisObj);
		if (poisEnd == std::string::npos)
		{
			p = poisKey + 20;
			continue;
		}

		/* Map key is "123": { … "points_of_interest". Name is the last "name" before pois. */
		const size_t winStart = (poisKey > 4000) ? (poisKey - 4000) : 0;
		long long mapId = -1;
		std::string mapName;
		size_t scan = winStart;
		while (scan < poisKey)
		{
			size_t nameAt = body.find("\"name\"", scan);
			if (nameAt == std::string::npos || nameAt >= poisKey) break;
			std::string n = JsonStringAfterKey(body, "name", nameAt);
			if (!n.empty()) mapName = std::move(n);
			scan = nameAt + 6;
		}
		{
			int depth = 0;
			bool inStr = false;
			for (size_t i = poisKey; i > 0; )
			{
				--i;
				const char c = body[i];
				if (c == '"')
				{
					size_t bs = 0;
					for (size_t j = i; j > 0 && body[j - 1] == '\\'; --j) ++bs;
					if ((bs % 2) == 0) inStr = !inStr;
					continue;
				}
				if (inStr) continue;
				if (c == '}') ++depth;
				else if (c == '{')
				{
					if (depth == 0)
					{
						size_t from = (i > 48) ? (i - 48) : 0;
						std::string chunk = body.substr(from, i - from);
						size_t q2 = chunk.rfind('"');
						if (q2 != std::string::npos && q2 > 0)
						{
							size_t q1 = chunk.rfind('"', q2 - 1);
							if (q1 != std::string::npos)
							{
								std::string key = chunk.substr(q1 + 1, q2 - q1 - 1);
								bool digits = !key.empty();
								for (char ch : key)
									if (ch < '0' || ch > '9') { digits = false; break; }
								if (digits)
									mapId = std::atoll(key.c_str());
							}
						}
						break;
					}
					--depth;
				}
			}
		}
		if (mapId <= 0)
		{
			/* Fallback: map-level "id" after pois (depth 1). */
			const size_t nextPois = body.find("\"points_of_interest\"", poisEnd + 1);
			const size_t idLimit = (nextPois == std::string::npos)
				? std::min(body.size(), poisEnd + 12000)
				: nextPois;
			int depth = 1;
			bool inStr = false, esc = false;
			for (size_t i = poisEnd + 1; i + 4 < idLimit && depth > 0; ++i)
			{
				const char c = body[i];
				if (inStr)
				{
					if (esc) esc = false;
					else if (c == '\\') esc = true;
					else if (c == '"') inStr = false;
					continue;
				}
				if (c == '"')
				{
					if (body.compare(i, 4, "\"id\"") == 0 && depth == 1)
					{
						const long long v = JsonIntAfterKey(body, "id", i);
						if (v > 0) mapId = v;
					}
					inStr = true;
					continue;
				}
				if (c == '{') ++depth;
				else if (c == '}') --depth;
			}
		}

		if (mapId > 0 && !mapName.empty())
		{
			/* Skip the container '{' so we iterate each nested POI object. */
			size_t pp = poisObj + 1;
			while (pp < poisEnd)
			{
				size_t brace = body.find('{', pp);
				if (brace == std::string::npos || brace >= poisEnd) break;
				const size_t end = JsonObjectEnd(body, brace);
				if (end == std::string::npos || end > poisEnd) break;
				WaypointsData::Poi poi;
				poi.mapId = static_cast<int>(mapId);
				poi.mapName = mapName;
				poi.id = static_cast<int>(JsonIntAfterKey(body, "id", brace));
				poi.name = JsonStringAfterKey(body, "name", brace);
				poi.type = JsonStringAfterKey(body, "type", brace);
				poi.chatLink = JsonStringAfterKey(body, "chat_link", brace);
				poi.hasCoord = JsonCoordAfterKey(body, "coord", brace,
					poi.continentX, poi.continentY);
				if (poi.id > 0 && !poi.chatLink.empty() && !poi.type.empty())
				{
					if (poi.name.empty())
						poi.name = poi.type;
					pois.push_back(std::move(poi));
				}
				pp = end + 1;
			}
		}
		p = poisEnd + 1;
	}
}

} // namespace WaypointsDataDetail
