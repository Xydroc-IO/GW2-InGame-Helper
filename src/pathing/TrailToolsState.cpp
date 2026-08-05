#include "TrailToolsShared.h"

#include "AddonPaths.h"
#include "Globals.h"

#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <wincrypt.h>

namespace TrailToolsDetail
{
	DraftPack gDraft{};
	bool      gPlaceOnce = false;
	bool      gFocus = false;
	int       gTab = 0;

	void SetStatus(const char* fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		std::vsnprintf(gDraft.status, sizeof(gDraft.status), fmt, ap);
		va_end(ap);
	}

	void SanitizePackName(char* name, size_t len)
	{
		if (!name || len == 0)
			return;
		size_t w = 0;
		for (size_t i = 0; name[i] && w + 1 < len; ++i)
		{
			const unsigned char c = static_cast<unsigned char>(name[i]);
			if (std::isalnum(c) || c == '_' || c == '-')
				name[w++] = static_cast<char>(c);
		}
		name[w] = 0;
		if (!name[0])
			std::snprintf(name, len, "ExamplePack");
	}

	std::string RootCategoryName()
	{
		char buf[64]{};
		std::snprintf(buf, sizeof(buf), "%s", gDraft.packName);
		for (char* p = buf; *p; ++p)
			*p = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
		return buf;
	}

	void SeedDefaultCategories()
	{
		const std::string rootName = RootCategoryName();
		gDraft.root = {};
		gDraft.root.name = rootName;
		gDraft.root.displayName = gDraft.displayName[0] ? gDraft.displayName : gDraft.packName;

		CategoryNode markers;
		markers.name = "m";
		markers.displayName = "Markers";
		markers.fadeFar = 3500.f;
		CategoryNode exm;
		exm.name = "exm";
		exm.displayName = "Example Marker";
		exm.iconFile = std::string("Data/") + gDraft.packName + "/Markers/ExampleMarker.png";
		markers.children.push_back(exm);
		gDraft.root.children.push_back(markers);

		CategoryNode trails;
		trails.name = "t";
		trails.displayName = "Trails";
		CategoryNode extrail;
		extrail.name = "extrail";
		extrail.displayName = "Example Trail";
		extrail.texture = std::string("Data/") + gDraft.packName + "/Markers/Trail.png";
		extrail.fadeNear = 3000.f;
		extrail.fadeFar = 3500.f;
		trails.children.push_back(extrail);
		gDraft.root.children.push_back(trails);

		std::snprintf(gDraft.markerType, sizeof(gDraft.markerType), "%s.m.exm", rootName.c_str());
		std::snprintf(gDraft.trailType, sizeof(gDraft.trailType), "%s.t.extrail", rootName.c_str());
		gDraft.active.type = gDraft.trailType;
		gDraft.active.fileRel = std::string("Data/") + gDraft.packName + "/Trails/Trail.trl";
	}

	std::string CategoryPath(const CategoryNode& node, const std::string& parentPath)
	{
		if (parentPath.empty())
			return node.name;
		return parentPath + "." + node.name;
	}

	void CollectLeafPaths(const CategoryNode& node, const std::string& parentPath,
		std::vector<std::string>& out, bool trailLeaves)
	{
		const std::string path = CategoryPath(node, parentPath);
		const bool isLeaf = node.children.empty();
		if (isLeaf)
		{
			const bool looksTrail = !node.texture.empty() ||
				(!parentPath.empty() && parentPath.find(".t") != std::string::npos) ||
				node.name.find("trail") != std::string::npos;
			const bool looksMarker = !node.iconFile.empty() || !looksTrail;
			if (trailLeaves ? looksTrail : looksMarker)
				out.push_back(path);
			return;
		}
		for (const CategoryNode& ch : node.children)
			CollectLeafPaths(ch, path, out, trailLeaves);
	}

	std::wstring AuthoringRoot()
	{
		return AddonPaths::EnsureUnder(AddonPaths::DataDir(), L"pathing\\authoring");
	}

	std::wstring PackDir()
	{
		SanitizePackName(gDraft.packName, sizeof(gDraft.packName));
		std::wstring rel = L"pathing\\authoring\\";
		for (const char* p = gDraft.packName; *p; ++p)
			rel.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
		return AddonPaths::EnsureUnder(AddonPaths::DataDir(), rel.c_str());
	}

	bool EnsureWorkspace()
	{
		const std::wstring pack = PackDir();
		if (pack.empty())
			return false;
		std::wstring markers = pack + L"\\Data\\";
		for (const char* p = gDraft.packName; *p; ++p)
			markers.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
		markers += L"\\Markers";
		AddonPaths::EnsureUnder(pack, L"Data");
		{
			std::wstring dataRel = L"Data\\";
			for (const char* p = gDraft.packName; *p; ++p)
				dataRel.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
			dataRel += L"\\Markers";
			AddonPaths::EnsureUnder(pack, dataRel.c_str());
			dataRel = L"Data\\";
			for (const char* p = gDraft.packName; *p; ++p)
				dataRel.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
			dataRel += L"\\Trails";
			AddonPaths::EnsureUnder(pack, dataRel.c_str());
		}
		return true;
	}

	bool OpenAuthoringFolder()
	{
		EnsureWorkspace();
		const std::wstring dir = PackDir();
		if (dir.empty())
			return false;
		return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", dir.c_str(),
			nullptr, nullptr, SW_SHOWNORMAL)) > 32;
	}

	void CopyClipboard(const char* text)
	{
		if (!text || !OpenClipboard(nullptr))
			return;
		EmptyClipboard();
		const size_t n = std::strlen(text) + 1;
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, n);
		if (mem)
		{
			void* p = GlobalLock(mem);
			if (p)
			{
				std::memcpy(p, text, n);
				GlobalUnlock(mem);
				SetClipboardData(CF_TEXT, mem);
			}
			else
				GlobalFree(mem);
		}
		CloseClipboard();
	}

	bool ReadMumblePose(uint32_t& mapId, float& x, float& y, float& z)
	{
		mapId = 0;
		x = y = z = 0.f;
		if (!G::Mumble || G::Mumble->uiTick == 0)
			return false;
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		if (!ctx || ctx->mapId == 0)
			return false;
		mapId = ctx->mapId;
		x = G::Mumble->fAvatarPosition[0];
		y = G::Mumble->fAvatarPosition[1];
		z = G::Mumble->fAvatarPosition[2];
		return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
	}

	std::string MakeGuidBase64()
	{
		GUID g{};
		if (FAILED(CoCreateGuid(&g)))
			return "AAAAAAAAAAAAAAAAAAAAAA==";
		DWORD len = 0;
		CryptBinaryToStringA(reinterpret_cast<const BYTE*>(&g), sizeof(g),
			CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &len);
		if (len == 0)
			return "AAAAAAAAAAAAAAAAAAAAAA==";
		std::string out(len, '\0');
		if (!CryptBinaryToStringA(reinterpret_cast<const BYTE*>(&g), sizeof(g),
				CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, out.data(), &len))
			return "AAAAAAAAAAAAAAAAAAAAAA==";
		while (!out.empty() && (out.back() == '\0' || out.back() == '\n' || out.back() == '\r'))
			out.pop_back();
		return out;
	}
}

namespace
{
	struct SeedOnce
	{
		SeedOnce() { TrailToolsDetail::SeedDefaultCategories(); }
	};
	SeedOnce gSeed;
}
