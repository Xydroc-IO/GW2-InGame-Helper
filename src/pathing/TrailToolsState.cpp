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
		WriteDefaultAssets();
		return true;
	}

	bool WriteBytesW(const std::wstring& path, const unsigned char* data, size_t len)
	{
		if (path.empty() || !data || len == 0)
			return false;
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, data, static_cast<DWORD>(len), &written, nullptr);
		CloseHandle(h);
		return ok && written == len;
	}

	bool WriteDefaultAssets()
	{
		/* Minimal 32×32 PNGs so packs have icons without a manual drop. */
		static const unsigned char kMarkerPng[] = {
			0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
			0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20,
			0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7a, 0x7a, 0xf4, 0x00, 0x00, 0x00,
			0x76, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0x60, 0x18, 0x05, 0x43,
			0x0d, 0x6c, 0xa9, 0x10, 0xb9, 0x83, 0x0f, 0xd3, 0xc5, 0xe2, 0xff, 0x27,
			0x34, 0xfe, 0x63, 0xc3, 0x34, 0x71, 0x08, 0x21, 0x4b, 0x09, 0x39, 0x66,
			0x40, 0x2c, 0xa7, 0x8a, 0x23, 0x28, 0xb5, 0x9c, 0x22, 0x47, 0x50, 0xcb,
			0x72, 0xb2, 0x1d, 0x41, 0x4d, 0xcb, 0x91, 0x1d, 0x31, 0x60, 0x96, 0x93,
			0xe4, 0x88, 0x01, 0x75, 0x00, 0xb5, 0xe3, 0x9e, 0xe4, 0xb4, 0x40, 0x4b,
			0xcb, 0x89, 0x0a, 0x85, 0x51, 0x07, 0x8c, 0x3a, 0x60, 0x50, 0x38, 0x60,
			0x40, 0xb3, 0xe1, 0x68, 0x49, 0x38, 0x28, 0x2a, 0xa3, 0x41, 0x51, 0x1d,
			0x0f, 0x78, 0x83, 0x64, 0x50, 0x34, 0xc9, 0x06, 0x45, 0xa3, 0x74, 0x50,
			0x34, 0xcb, 0x07, 0x4d, 0xc7, 0x64, 0x14, 0xd0, 0x0a, 0x00, 0x00, 0x09,
			0x72, 0xe2, 0xdc, 0xbd, 0x02, 0x72, 0xc1, 0x00, 0x00, 0x00, 0x00, 0x49,
			0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
		};
		static const unsigned char kTrailPng[] = {
			0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
			0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20,
			0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7a, 0x7a, 0xf4, 0x00, 0x00, 0x00,
			0x32, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0xed, 0xce, 0xb1, 0x0d, 0x00,
			0x30, 0x08, 0xc0, 0x30, 0x4e, 0xe4, 0x7f, 0x75, 0xe6, 0x0d, 0x78, 0xa1,
			0xac, 0xc8, 0x91, 0xb2, 0x3b, 0x42, 0xfa, 0x2c, 0x5f, 0xd7, 0x66, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x7b, 0x00,
			0x9d, 0x6d, 0x00, 0xd2, 0x80, 0x73, 0x4e, 0x5a, 0xfe, 0x47, 0x8e, 0x00,
			0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
		};

		std::wstring base = PackDir();
		base.push_back(L'\\');
		base += L"Data\\";
		for (const char* p = gDraft.packName; *p; ++p)
			base.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
		base += L"\\Markers\\";

		const std::wstring markerPath = base + L"ExampleMarker.png";
		const std::wstring trailPath = base + L"Trail.png";
		bool ok = true;
		if (GetFileAttributesW(markerPath.c_str()) == INVALID_FILE_ATTRIBUTES)
			ok = WriteBytesW(markerPath, kMarkerPng, sizeof(kMarkerPng)) && ok;
		if (GetFileAttributesW(trailPath.c_str()) == INVALID_FILE_ATTRIBUTES)
			ok = WriteBytesW(trailPath, kTrailPng, sizeof(kTrailPng)) && ok;
		return ok;
	}

	bool HasDraftPreview()
	{
		if (!gDraft.previewEnabled)
			return false;
		if (gDraft.active.points.size() >= 2 && gDraft.active.mapId != 0)
			return true;
		uint32_t mapId = 0;
		float x = 0.f, y = 0.f, z = 0.f;
		if (!ReadMumblePose(mapId, x, y, z))
			return !gDraft.pois.empty();
		for (const DraftPoi& p : gDraft.pois)
		{
			if (p.mapId == mapId)
				return true;
		}
		return false;
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
		static bool sCom;
		if (!sCom)
		{
			CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			sCom = true;
		}
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
