#include "TrailToolsShared.h"
#include "TrailToolsTrl.h"
#include "PathingParse.h"
#include "AddonPaths.h"

#include "miniz/miniz.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace
{
	std::string WideToUtf8(const std::wstring& w)
	{
		if (w.empty())
			return {};
		const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (n <= 0)
			return {};
		std::string s(static_cast<size_t>(n - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
		return s;
	}

	std::wstring Utf8ToWide(const std::string& s)
	{
		if (s.empty())
			return {};
		const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
		if (n <= 0)
			return {};
		std::wstring w(static_cast<size_t>(n - 1), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
		return w;
	}

	bool EnsureDirDeep(const std::wstring& path)
	{
		size_t start = 0;
		while (start < path.size())
		{
			size_t slash = path.find(L'\\', start);
			if (slash == std::wstring::npos)
				slash = path.size();
			const std::wstring sub = path.substr(0, slash);
			if (!sub.empty() && sub.back() != L':')
				CreateDirectoryW(sub.c_str(), nullptr);
			if (slash >= path.size())
				break;
			start = slash + 1;
		}
		return true;
	}

	bool WriteBytes(const std::wstring& path, const void* data, size_t len)
	{
		const size_t slash = path.find_last_of(L'\\');
		if (slash != std::wstring::npos)
			EnsureDirDeep(path.substr(0, slash));
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD w = 0;
		const BOOL ok = WriteFile(h, data, static_cast<DWORD>(len), &w, nullptr);
		CloseHandle(h);
		return ok && w == len;
	}

	std::string FileStem(const std::wstring& path)
	{
		std::string n = WideToUtf8(path);
		const size_t slash = n.find_last_of("/\\");
		if (slash != std::string::npos)
			n = n.substr(slash + 1);
		const size_t dot = n.find_last_of('.');
		if (dot != std::string::npos)
			n = n.substr(0, dot);
		return n;
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

	TrailToolsDetail::CategoryNode StyleToNode(const std::string& name,
		const PathingParse::MarkerStyle& st)
	{
		TrailToolsDetail::CategoryNode n;
		n.name = name;
		n.displayName = name;
		if (st.hasIconFile)
			n.iconFile = st.iconFile;
		if (st.hasTexture)
			n.texture = st.texture;
		if (st.hasFadeNear)
			n.fadeNear = st.fadeNear;
		if (st.hasFadeFar)
			n.fadeFar = st.fadeFar;
		if (st.hasTrailScale)
			n.trailScale = st.trailScale;
		if (st.hasIconSize)
			n.iconSize = st.iconSize;
		if (st.hasAlpha)
			n.alpha = st.alpha;
		if (st.hasColor)
			n.color = st.color;
		if (st.hasSchedule)
			n.schedule = st.schedule;
		if (st.hasScheduleDuration)
			n.scheduleDuration = st.scheduleDuration;
		return n;
	}

	void InsertCategoryPath(TrailToolsDetail::CategoryNode& root,
		const std::string& path, const PathingParse::MarkerStyle& style)
	{
		std::string rem = path;
		TrailToolsDetail::CategoryNode* cur = &root;
		while (!rem.empty())
		{
			size_t dot = rem.find('.');
			const std::string seg = rem.substr(0, dot);
			rem = (dot == std::string::npos) ? std::string() : rem.substr(dot + 1);
			TrailToolsDetail::CategoryNode* child = nullptr;
			for (auto& ch : cur->children)
			{
				if (ch.name == seg)
				{
					child = &ch;
					break;
				}
			}
			if (!child)
			{
				cur->children.push_back(StyleToNode(seg, {}));
				child = &cur->children.back();
			}
			if (rem.empty())
			{
				/* leaf — apply style */
				TrailToolsDetail::CategoryNode leaf = StyleToNode(seg, style);
				leaf.children = child->children;
				*child = std::move(leaf);
			}
			cur = child;
		}
	}
}

bool TrailToolsDetail::ImportTacoToDraft(const std::wstring& tacoPath, std::string& err)
{
	std::vector<uint8_t> file;
	if (!PathingParse::ReadFileW(tacoPath, file, PathingParse::kMaxZipBytes) || file.empty())
	{
		err = "Could not read .taco.";
		return false;
	}
	mz_zip_archive zip{};
	memset(&zip, 0, sizeof(zip));
	if (!mz_zip_reader_init_mem(&zip, file.data(), file.size(), 0))
	{
		err = "Invalid zip/.taco.";
		return false;
	}

	const std::string packStem = FileStem(tacoPath);
	char nameBuf[64]{};
	std::snprintf(nameBuf, sizeof(nameBuf), "%s", packStem.c_str());
	SanitizePackName(nameBuf, sizeof(nameBuf));
	std::snprintf(gDraft.packName, sizeof(gDraft.packName), "%s", nameBuf);
	std::snprintf(gDraft.displayName, sizeof(gDraft.displayName), "%s", packStem.c_str());
	EnsureWorkspace();

	/* Extract all files into authoring workspace. */
	const std::wstring destRoot = PackDir();
	const mz_uint n = mz_zip_reader_get_num_files(&zip);
	std::string mainXml;
	for (mz_uint i = 0; i < n; ++i)
	{
		mz_zip_archive_file_stat st{};
		if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory)
			continue;
		std::string entry = st.m_filename ? st.m_filename : "";
		std::replace(entry.begin(), entry.end(), '\\', '/');
		std::vector<uint8_t> bytes;
		if (!PathingParse::ZipExtractIndex(zip, static_cast<int>(i), bytes,
			PathingParse::kMaxZipBytes))
			continue;
		std::wstring out = destRoot;
		out.push_back(L'\\');
		for (char c : entry)
			out.push_back(c == '/' ? L'\\' : static_cast<wchar_t>(static_cast<unsigned char>(c)));
		WriteBytes(out, bytes.data(), bytes.size());

		const std::string low = PathingParse::ToLower(entry);
		if (low.size() >= 4 && low.compare(low.size() - 4, 4, ".xml") == 0 &&
			low.find("overlay") == std::string::npos)
		{
			if (mainXml.empty() || low.find(PathingParse::ToLower(packStem)) != std::string::npos)
				mainXml.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
		}
	}

	if (mainXml.empty())
	{
		/* Fallback: largest xml */
		for (mz_uint i = 0; i < n; ++i)
		{
			mz_zip_archive_file_stat st{};
			if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory)
				continue;
			std::string entry = st.m_filename ? st.m_filename : "";
			std::string low = PathingParse::ToLower(entry);
			if (low.size() < 4 || low.compare(low.size() - 4, 4, ".xml") != 0)
				continue;
			std::vector<uint8_t> bytes;
			if (!PathingParse::ZipExtractIndex(zip, static_cast<int>(i), bytes,
				PathingParse::kMaxZipBytes))
				continue;
			if (bytes.size() > mainXml.size())
				mainXml.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
		}
	}
	mz_zip_reader_end(&zip);

	if (mainXml.empty())
	{
		err = "No XML found in pack.";
		return false;
	}

	std::vector<PathingTrails::Category> menu;
	std::unordered_map<std::string, PathingParse::MarkerStyle> styles;
	PathingParse::ParseMarkerMenuXml(mainXml, menu, styles);

	gDraft.root = {};
	gDraft.root.name = RootCategoryName();
	gDraft.root.displayName = gDraft.displayName;
	for (const auto& kv : styles)
		InsertCategoryPath(gDraft.root, kv.first, kv.second);

	std::unordered_map<std::string, uint32_t> catMaps;
	PathingParse::CollectCategoryMapIds(mainXml, catMaps);
	std::vector<PathingParse::IndexedPoi> pois;
	PathingParse::IndexPoisXml(tacoPath, mainXml, pois, catMaps);
	gDraft.pois.clear();
	for (const auto& ip : pois)
	{
		DraftPoi p;
		p.mapId = ip.mapId;
		p.x = ip.wx;
		p.y = ip.wy;
		p.z = ip.wz;
		p.type = ip.type;
		p.guid = ip.guid;
		if (ip.style.hasBehavior)
			p.behavior = ip.style.behavior;
		if (ip.style.hasAutoTrigger)
			p.autoTrigger = ip.style.autoTrigger;
		if (ip.style.hasTriggerRange)
			p.triggerRange = ip.style.triggerRange;
		if (ip.style.hasTipName)
			p.tipName = ip.style.tipName;
		if (ip.style.hasTipDescription)
			p.tipDescription = ip.style.tipDescription;
		if (ip.style.hasInfo)
			p.info = ip.style.info;
		if (ip.style.hasCopy)
			p.copy = ip.style.copy;
		if (ip.style.hasSchedule)
			p.schedule = ip.style.schedule;
		if (ip.style.hasScheduleDuration)
			p.scheduleDuration = ip.style.scheduleDuration;
		if (ip.style.hasIconFile)
			p.iconFile = ip.style.iconFile;
		if (ip.style.hasScriptOnce)
			p.scriptOnce = ip.style.scriptOnce;
		if (ip.style.hasScriptTrigger)
			p.scriptTrigger = ip.style.scriptTrigger;
		if (ip.style.hasScriptFilter)
			p.scriptFilter = ip.style.scriptFilter;
		if (ip.style.hasScriptTick)
			p.scriptTick = ip.style.scriptTick;
		if (ip.style.hasScriptFocus)
			p.scriptFocus = ip.style.scriptFocus;
		if (ip.style.hasHide)
			p.hide = ip.style.hide;
		if (ip.style.hasShow)
			p.show = ip.style.show;
		if (ip.style.hasCopyMessage)
			p.copyMessage = ip.style.copyMessage;
		if (ip.style.hasResetLength)
			p.resetLength = ip.style.resetLength;
		if (ip.style.hasInvertBehavior)
			p.invertBehavior = ip.style.invertBehavior;
		gDraft.pois.push_back(std::move(p));
	}

	std::vector<PathingParse::IndexedTrail> trails;
	PathingParse::IndexXml(tacoPath, mainXml, trails);
	gDraft.trails.clear();
	gDraft.active = {};
	for (const auto& it : trails)
	{
		DraftTrail dt;
		dt.fileRel = it.entryName;
		dt.type = it.type;
		dt.mapId = it.mapId;
		std::wstring trlPath = PackDir();
		trlPath.push_back(L'\\');
		for (char c : it.entryName)
			trlPath.push_back(c == '/' ? L'\\' : static_cast<wchar_t>(static_cast<unsigned char>(c)));
		uint32_t mid = 0;
		std::vector<PathingTrails::WorldPoint> pts;
		if (TrailToolsTrl::Read(trlPath, mid, pts))
		{
			dt.mapId = mid;
			dt.points = std::move(pts);
		}
		gDraft.trails.push_back(dt);
	}
	if (!gDraft.trails.empty())
	{
		gDraft.active = gDraft.trails[0];
		gDraft.selectedTrail = 0;
		std::snprintf(gDraft.trailType, sizeof(gDraft.trailType), "%s",
			gDraft.active.type.c_str());
	}
	if (!gDraft.pois.empty())
	{
		std::snprintf(gDraft.markerType, sizeof(gDraft.markerType), "%s",
			gDraft.pois[0].type.c_str());
	}

	(void)Utf8ToWide; /* silence if unused on some builds */
	SetStatus("Imported %s (%zu POIs, %zu trails).", packStem.c_str(),
		gDraft.pois.size(), gDraft.trails.size());
	return true;
}
