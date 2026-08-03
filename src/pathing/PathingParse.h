#pragma once

#include "PathingTrails.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "miniz/miniz.h"

/* Zip / XML / .trl / .taco parsing helpers for PathingTrails (no overlay state). */
namespace PathingParse
{
	constexpr size_t kMaxZipBytes = 120u * 1024u * 1024u;
	constexpr size_t kMaxTrailFile = 8u * 1024u * 1024u;
	constexpr size_t kMaxPointsPerTrail = 4096;

	struct MarkerStyle
	{
		std::string iconFile;
		bool hasIconFile = false;
		std::string texture;
		bool hasTexture = false;
		bool minimapVisible = true;
		bool hasMinimapVisible = false;
		bool mapVisible = true;
		bool hasMapVisible = false;
		bool inGameVisible = true;
		bool hasInGameVisible = false;
		float mapDisplaySize = 20.f;
		bool hasMapDisplaySize = false;
		float minSize = 5.f;
		bool hasMinSize = false;
		float maxSize = 2048.f;
		bool hasMaxSize = false;
		float iconSize = 1.f;
		bool hasIconSize = false;
		float heightOffset = 1.5f;
		bool hasHeightOffset = false;
		float fadeNear = -1.f;
		bool hasFadeNear = false;
		float fadeFar = -1.f;
		bool hasFadeFar = false;
		float alpha = 1.f;
		bool hasAlpha = false;
		float trailScale = 1.f;
		bool hasTrailScale = false;
		uint32_t color = 0xFFFFFFFFu;
		bool hasColor = false;

		/* Blish Pathing / TacO runtime (category-inheritable). */
		int behavior = 0;
		bool hasBehavior = false;
		bool autoTrigger = false;
		bool hasAutoTrigger = false;
		float triggerRange = 2.f;
		bool hasTriggerRange = false;
		float resetLength = 0.f;
		bool hasResetLength = false;
		bool invertBehavior = false;
		bool hasInvertBehavior = false;
		std::string hide; /* comma-separated category paths */
		bool hasHide = false;
		std::string show;
		bool hasShow = false;
		std::string tipName;
		bool hasTipName = false;
		std::string tipDescription;
		bool hasTipDescription = false;
		std::string info;
		bool hasInfo = false;
		std::string copy;
		bool hasCopy = false;
		std::string copyMessage;
		bool hasCopyMessage = false;
	};

	struct IndexedTrail
	{
		std::wstring packPath;
		std::string  entryName;
		std::string  type;
		uint32_t     color = 0xFFFFFFFFu;
		uint32_t     mapId = 0; /* from .trl header; 0 = unknown */
		int          fileIndex = -1; /* zip central-dir index within its pack */
		bool         mapCompletion = false;
		MarkerStyle  style;
	};

	struct IndexedPoi
	{
		std::wstring packPath;
		std::string  type;
		std::string  guid; /* Blish/TacO GUID — behavior persistence key */
		uint32_t     mapId = 0;
		float        wx = 0.f;
		float        wy = 0.f;
		float        wz = 0.f;
		MarkerStyle  style;
	};

	std::string ToLower(std::string s);
	bool LooksLikeMapCompletion(const std::string& type, const std::string& path);
	void DecodeXmlEntities(std::string& text);

	uint32_t ParseColorAttr(const std::string& tag);
	std::string Attr(const std::string& tag, const char* key);
	bool ParseBoolValue(const std::string& value, bool fallback = true);
	void MergeStyle(MarkerStyle& dst, const MarkerStyle& src);
	MarkerStyle ParseStyle(const std::string& tag);
	MarkerStyle ResolveStyle(
		const std::string& type,
		const MarkerStyle& own,
		const std::unordered_map<std::string, MarkerStyle>& categories);

	bool ReadFileW(const std::wstring& path, std::vector<uint8_t>& out, size_t maxBytes);
	int ZipLocate(mz_zip_archive& zip, const std::string& entryName);
	bool ZipExtractIndex(mz_zip_archive& zip, int fileIndex,
		std::vector<uint8_t>& out, size_t maxOut);
	bool ZipExtractEntry(mz_zip_archive& zip, const std::string& entryName,
		std::vector<uint8_t>& out, size_t maxOut);
	bool ZipReadEntry(const std::wstring& zipPath, const std::string& entryName,
		std::vector<uint8_t>& out, size_t maxOut);
	uint32_t PeekTrlMapId(mz_zip_archive& zip, int fileIndex);

	void IndexXml(const std::wstring& packPath, const std::string& xml,
		std::vector<IndexedTrail>& out);
	/* categoryMapIds: MarkerCategory path → MapID (Blish inherits onto child POIs). */
	void CollectCategoryMapIds(const std::string& xml,
		std::unordered_map<std::string, uint32_t>& categoryMapIds);
	void IndexPoisXml(const std::wstring& packPath, const std::string& xml,
		std::vector<IndexedPoi>& out,
		const std::unordered_map<std::string, uint32_t>& categoryMapIds);
	bool ParseTrl(const std::vector<uint8_t>& data, uint32_t& mapId,
		std::vector<PathingTrails::WorldPoint>& world);

	void ParseMarkerMenuXml(
		const std::string& xml,
		std::vector<PathingTrails::Category>& roots,
		std::unordered_map<std::string, MarkerStyle>& styles);
}
