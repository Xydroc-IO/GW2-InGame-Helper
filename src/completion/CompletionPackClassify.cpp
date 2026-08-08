#include "CompletionInternal.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "UiAscii.h"

namespace CompletionDetail
{
	namespace
	{
		constexpr uint32_t kPackIdBit = 0x80000000u;

		std::string ToLower(std::string s)
		{
			for (char& c : s)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return s;
		}

		bool EndsWith(const std::string& s, const char* suf)
		{
			const size_t n = std::strlen(suf);
			return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
		}

		bool PackLeafHas(const char* leaf, const char* needle)
		{
			if (!leaf || !leaf[0] || !needle)
				return false;
			return ToLower(leaf).find(ToLower(needle)) != std::string::npos;
		}

		bool IsLadyApPack(const char* packLeaf)
		{
			return PackLeafHas(packLeaf, "ladyelyssaap");
		}

		bool IsLadyHeartType(const std::string& typeLow)
		{
			if (typeLow.find("heartpath") != std::string::npos)
				return true;
			if (typeLow.find(".heartinfo") != std::string::npos)
				return true;
			return EndsWith(typeLow, ".heartinfo");
		}

		bool IsTekkitHeartType(const std::string& typeLow)
		{
			if (typeLow.find("tw_guides.tw_mc") == std::string::npos &&
				typeLow.find("tw_mc") == std::string::npos)
				return false;
			return typeLow.find("heart") != std::string::npos;
		}

		bool IsLadyHeroType(const std::string& typeLow)
		{
			return typeLow == "legs.hp" || typeLow == "leag.hp" ||
				(typeLow.size() > 8 && typeLow.compare(0, 8, "legs.hp.") == 0) ||
				(typeLow.size() > 8 && typeLow.compare(0, 8, "leag.hp.") == 0);
		}

		bool IsTekkitHeroType(const std::string& typeLow)
		{
			if (typeLow.find("tw_") == std::string::npos)
				return false;
			return typeLow.find("heropoint") != std::string::npos ||
				typeLow.find("hero_point") != std::string::npos ||
				typeLow.find(".hp.") != std::string::npos ||
				EndsWith(typeLow, ".hp");
		}

		bool IsMapCompletionExcludedApType(const std::string& typeLow)
		{
			if (typeLow == "leag.map" || typeLow == "legs.map" ||
				(typeLow.size() > 9 && typeLow.compare(0, 9, "leag.map.") == 0) ||
				(typeLow.size() > 9 && typeLow.compare(0, 9, "legs.map.") == 0))
				return true;
			if (IsLadyHeroType(typeLow) || IsLadyHeartType(typeLow))
				return true;
			return false;
		}

		bool IsAchievementType(const char* type, const char* packLeaf)
		{
			const std::string typeLow = ToLower(type ? type : "");
			if (typeLow.empty())
				return false;
			if (IsLadyApPack(packLeaf))
				return !IsMapCompletionExcludedApType(typeLow);
			if (typeLow == "leag" || (typeLow.size() > 5 && typeLow.compare(0, 5, "leag.") == 0))
				return !IsMapCompletionExcludedApType(typeLow);
			return false;
		}

		uint32_t Fnv1a32(const char* s)
		{
			uint32_t h = 2166136261u;
			if (!s)
				return h;
			for (; *s; ++s)
			{
				h ^= static_cast<uint8_t>(*s);
				h *= 16777619u;
			}
			return h;
		}

		bool LadyPreferredPackLeaf(const char* packLeaf)
		{
			return PackLeafHas(packLeaf, "ladyelyssa") && !IsLadyApPack(packLeaf);
		}
	}

	bool IsLadyPreferredPackLeaf(const char* packLeaf)
	{
		return LadyPreferredPackLeaf(packLeaf);
	}

	PackMarkerKind ClassifyPackMarker(const char* type, const char* packLeaf)
	{
		if (IsAchievementType(type, packLeaf))
			return PackMarkerKind::Achievement;
		const std::string typeLow = ToLower(type ? type : "");
		if (IsLadyHeartType(typeLow) || IsTekkitHeartType(typeLow))
			return PackMarkerKind::Heart;
		if (IsLadyHeroType(typeLow) || IsTekkitHeroType(typeLow))
			return PackMarkerKind::Hero;
		return PackMarkerKind::None;
	}

	uint32_t StablePackObjectiveId(const char* guid, const char* type, uint32_t mapId,
		float wx, float wz)
	{
		char key[384]{};
		if (guid && guid[0])
			std::snprintf(key, sizeof(key), "g:%s", guid);
		else
		{
			const int rx = static_cast<int>(std::lround(wx * 10.f));
			const int rz = static_cast<int>(std::lround(wz * 10.f));
			std::snprintf(key, sizeof(key), "t:%s|%u|%d|%d", type ? type : "", mapId, rx, rz);
		}
		return (Fnv1a32(key) & ~kPackIdBit) | kPackIdBit;
	}

	void FormatPackMarkerName(const char* tipName, const char* type, uint32_t mapId,
		char* out, size_t outLen)
	{
		if (!out || outLen == 0)
			return;
		out[0] = '\0';
		if (tipName && tipName[0])
		{
			UiAscii::SanitizeForUi(out, outLen, tipName);
			if (out[0])
				return;
		}
		const char* leaf = type ? type : "";
		if (type)
		{
			if (const char* dot = std::strrchr(type, '.'))
				leaf = dot + 1;
		}
		if (leaf && leaf[0])
			UiAscii::SanitizeForUi(out, outLen, leaf);
		if (!out[0])
			std::snprintf(out, outLen, "Marker %u", mapId);
	}
}
