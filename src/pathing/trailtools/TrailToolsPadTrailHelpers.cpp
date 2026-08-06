#include "TrailToolsInternal.h"
#include "TrailToolsShared.h"
#include "TrailToolsTrl.h"

#include "Settings.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>
#include <commdlg.h>

namespace TrailToolsDetail
{
	std::wstring Utf8ToWide(const char* u)
	{
		if (!u || !*u)
			return {};
		const int n = MultiByteToWideChar(CP_UTF8, 0, u, -1, nullptr, 0);
		if (n <= 0)
			return {};
		std::wstring w(static_cast<size_t>(n - 1), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, u, -1, w.data(), n);
		return w;
	}

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

	std::wstring PackRelToAbs(const std::string& fileRel)
	{
		std::wstring p = PackDir();
		p.push_back(L'\\');
		for (char c : fileRel)
			p.push_back(c == '/' ? L'\\' : static_cast<wchar_t>(static_cast<unsigned char>(c)));
		return p;
	}

	std::wstring TrailsFolder()
	{
		std::wstring p = PackDir();
		p += L"\\Data\\";
		for (const char* c = gDraft.packName; *c; ++c)
			p.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*c)));
		p += L"\\Trails";
		return p;
	}

	std::wstring ActiveTrlPath()
	{
		return PackRelToAbs(gDraft.active.fileRel);
	}

	void RememberDirFromPath(const std::wstring& fullPath)
	{
		const size_t slash = fullPath.find_last_of(L"\\/");
		if (slash == std::wstring::npos)
			return;
		const std::string dir = WideToUtf8(fullPath.substr(0, slash));
		std::snprintf(gDraft.lastTrlDir, sizeof(gDraft.lastTrlDir), "%s", dir.c_str());
		Settings::SetDirty();
	}

	std::wstring DialogStartDir()
	{
		if (gDraft.lastTrlDir[0])
			return Utf8ToWide(gDraft.lastTrlDir);
		return TrailsFolder();
	}

	bool IsSectionBreak(const PathingTrails::WorldPoint& p)
	{
		return p.x == 0.f && p.y == 0.f && p.z == 0.f;
	}

	bool TryAbsUnderPack(const std::wstring& absPath, std::string& outRel)
	{
		std::wstring pack = PackDir();
		if (pack.empty() || absPath.size() < pack.size() + 2)
			return false;
		/* Case-insensitive prefix on Windows. */
		std::wstring abs = absPath;
		std::wstring root = pack;
		for (auto& ch : abs)
			if (ch == L'/')
				ch = L'\\';
		for (auto& ch : root)
			if (ch == L'/')
				ch = L'\\';
		for (size_t i = 0; i < root.size(); ++i)
		{
			const wchar_t a = abs[i] >= L'A' && abs[i] <= L'Z' ? abs[i] + 32 : abs[i];
			const wchar_t b = root[i] >= L'A' && root[i] <= L'Z' ? root[i] + 32 : root[i];
			if (a != b)
				return false;
		}
		if (abs[root.size()] != L'\\')
			return false;
		std::string rel = WideToUtf8(abs.substr(root.size() + 1));
		for (char& c : rel)
			if (c == '\\')
				c = '/';
		outRel = std::move(rel);
		return true;
	}

	void RegisterActiveInPack()
	{
		UpsertActiveTrailInPack();
	}

	bool SaveActiveToPath(const std::wstring& path)
	{
		SyncActiveType();
		if (gDraft.active.mapId == 0 || gDraft.active.points.size() < 2)
		{
			SetStatus("Need map + at least 2 points to save.");
			return false;
		}
		/* Ensure parent folder exists. */
		{
			const size_t slash = path.find_last_of(L"\\/");
			if (slash != std::wstring::npos)
			{
				const std::wstring dir = path.substr(0, slash);
				CreateDirectoryW(dir.c_str(), nullptr);
			}
		}
		if (!TrailToolsTrl::Write(path, gDraft.active.mapId, gDraft.active.points))
		{
			SetStatus("Save failed.");
			return false;
		}
		RememberDirFromPath(path);
		std::string under;
		if (TryAbsUnderPack(path, under))
		{
			gDraft.active.fileRel = under;
			ApplyStemFromFileRel();
		}
		else
			SyncActiveFileRelFromStem();
		RegisterActiveInPack();
		SetStatus("Saved %s (%zu pts).", gDraft.active.fileRel.c_str(), gDraft.active.points.size());
		return true;
	}

	bool DialogPickTrl(bool saveAs, std::wstring& outPath)
	{
		EnsureWorkspace();
		CreateDirectoryW(TrailsFolder().c_str(), nullptr);

		wchar_t fileBuf[MAX_PATH]{};
		if (!saveAs)
			fileBuf[0] = L'\0';
		else
		{
			const char* stem = gDraft.trailFileStem[0] ? gDraft.trailFileStem : "Trail";
			const std::wstring stemW = Utf8ToWide(stem);
			std::swprintf(fileBuf, MAX_PATH, L"%ls.trl", stemW.c_str());
		}

		const std::wstring start = DialogStartDir();
		wchar_t dirBuf[MAX_PATH]{};
		std::wcsncpy(dirBuf, start.c_str(), MAX_PATH - 1);

		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = nullptr;
		ofn.lpstrFilter = L"Trail files (*.trl)\0*.trl\0All files (*.*)\0*.*\0";
		ofn.lpstrFile = fileBuf;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrInitialDir = dirBuf;
		ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
			(saveAs ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
		ofn.lpstrDefExt = L"trl";
		ofn.lpstrTitle = saveAs ? L"Save trail as" : L"Load trail";

		const BOOL ok = saveAs ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
		if (!ok)
			return false;
		outPath.assign(fileBuf);
		return true;
	}
}
