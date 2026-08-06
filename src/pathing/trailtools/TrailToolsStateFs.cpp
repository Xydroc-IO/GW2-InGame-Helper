#include "TrailToolsShared.h"

#include "AddonPaths.h"
#include "Globals.h"

#include <cmath>
#include <cstring>

#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <wincrypt.h>

#include "TrailToolsLooks.inc"

namespace TrailToolsDetail
{
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

	static bool WriteBytesW(const std::wstring& path, const unsigned char* data, size_t len)
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
		size_t n = 0;
		const TrailToolsLooks::EmbeddedPng* all = TrailToolsLooks::All(&n);

		std::wstring base = PackDir();
		base.push_back(L'\\');
		base += L"Data\\";
		for (const char* p = gDraft.packName; *p; ++p)
			base.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
		base += L"\\Markers\\";

		bool ok = true;
		for (size_t i = 0; i < n; ++i)
		{
			std::wstring path = base;
			for (const char* c = all[i].file; *c; ++c)
				path.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*c)));
			if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
				ok = WriteBytesW(path, all[i].data, all[i].len) && ok;
		}
		/* Compat aliases used by older drafts. */
		const std::wstring disc = base + L"Marker_Disc.png";
		const std::wstring chev = base + L"Trail_Chevron.png";
		const std::wstring aliasM = base + L"ExampleMarker.png";
		const std::wstring aliasT = base + L"Trail.png";
		if (GetFileAttributesW(aliasM.c_str()) == INVALID_FILE_ATTRIBUTES &&
			GetFileAttributesW(disc.c_str()) != INVALID_FILE_ATTRIBUTES)
			CopyFileW(disc.c_str(), aliasM.c_str(), FALSE);
		if (GetFileAttributesW(aliasT.c_str()) == INVALID_FILE_ATTRIBUTES &&
			GetFileAttributesW(chev.c_str()) != INVALID_FILE_ATTRIBUTES)
			CopyFileW(chev.c_str(), aliasT.c_str(), FALSE);
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
