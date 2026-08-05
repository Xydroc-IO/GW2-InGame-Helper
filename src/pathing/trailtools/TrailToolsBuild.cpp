#include "TrailToolsBuild.h"

#include "AddonPaths.h"
#include "TrailToolsShared.h"
#include "TrailToolsTrl.h"
#include "TrailToolsXml.h"

#include "miniz/miniz.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace
{
	std::wstring RelToPackPath(const std::string& rel)
	{
		std::wstring p = TrailToolsDetail::PackDir();
		p.push_back(L'\\');
		for (char c : rel)
			p.push_back(c == '/' ? L'\\' : static_cast<wchar_t>(static_cast<unsigned char>(c)));
		return p;
	}

	bool EnsureParentDirs(const std::wstring& filePath)
	{
		const size_t slash = filePath.find_last_of(L"\\/");
		if (slash == std::wstring::npos || slash == 0)
			return true;
		std::wstring cur;
		for (size_t i = 0; i < slash; ++i)
		{
			cur.push_back(filePath[i]);
			if (filePath[i] == L'\\' || filePath[i] == L'/' || i + 1 == slash)
			{
				if (cur.size() >= 3 && cur.back() != L':' &&
					!(cur.size() == 2 && cur[1] == L':'))
					CreateDirectoryW(cur.c_str(), nullptr);
			}
		}
		return true;
	}

	std::string WideToUtf8(const std::wstring& w)
	{
		if (w.empty())
			return {};
		const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (n <= 0)
			return {};
		std::string o(static_cast<size_t>(n - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, o.data(), n, nullptr, nullptr);
		return o;
	}

	bool PathPrefixMatch(const std::wstring& full, const std::wstring& root)
	{
		if (full.size() < root.size())
			return false;
		for (size_t i = 0; i < root.size(); ++i)
		{
			wchar_t a = full[i], b = root[i];
			if (a >= L'A' && a <= L'Z') a = static_cast<wchar_t>(a - L'A' + L'a');
			if (b >= L'A' && b <= L'Z') b = static_cast<wchar_t>(b - L'A' + L'a');
			if (a == L'/') a = L'\\';
			if (b == L'/') b = L'\\';
			if (a != b)
				return false;
		}
		return true;
	}

	std::string ZipEntryName(const std::wstring& packRoot, const std::wstring& filePath)
	{
		std::wstring root = packRoot;
		if (!root.empty() && root.back() != L'\\' && root.back() != L'/')
			root.push_back(L'\\');
		std::wstring rel = filePath;
		if (PathPrefixMatch(filePath, root))
			rel = filePath.substr(root.size());
		std::string entry = WideToUtf8(rel);
		for (char& c : entry)
		{
			if (c == '\\')
				c = '/';
		}
		return entry;
	}

	bool SkipZipEntry(const std::string& entry)
	{
		/* Authoring sidecars must not ship — Pathing indexes every .xml in the taco. */
		if (entry == "_draft_session.xml")
			return true;
		if (entry.size() >= 18 &&
			entry.compare(entry.size() - 18, 18, "/_draft_session.xml") == 0)
			return true;
		return false;
	}

	bool AddFileToZip(mz_zip_archive& zip, const std::wstring& packRoot, const std::wstring& filePath)
	{
		const std::string entry = ZipEntryName(packRoot, filePath);
		if (entry.empty() || SkipZipEntry(entry))
			return true;
		HANDLE h = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 0 || sz.QuadPart > 32 * 1024 * 1024)
		{
			CloseHandle(h);
			return false;
		}
		std::vector<uint8_t> buf(static_cast<size_t>(sz.QuadPart));
		DWORD got = 0;
		const BOOL ok = buf.empty() ||
			(ReadFile(h, buf.data(), static_cast<DWORD>(buf.size()), &got, nullptr) &&
				got == buf.size());
		CloseHandle(h);
		if (!ok)
			return false;
		return mz_zip_writer_add_mem(&zip, entry.c_str(), buf.data(), buf.size(),
			MZ_DEFAULT_COMPRESSION) != 0;
	}

	bool WalkAdd(mz_zip_archive& zip, const std::wstring& packRoot, const std::wstring& dir)
	{
		std::wstring pattern = dir;
		if (!pattern.empty() && pattern.back() != L'\\')
			pattern.push_back(L'\\');
		pattern += L"*";
		WIN32_FIND_DATAW fd{};
		HANDLE find = FindFirstFileW(pattern.c_str(), &fd);
		if (find == INVALID_HANDLE_VALUE)
			return true;
		bool ok = true;
		do
		{
			if (fd.cFileName[0] == L'.' &&
				(fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0)))
				continue;
			std::wstring child = dir;
			if (!child.empty() && child.back() != L'\\')
				child.push_back(L'\\');
			child += fd.cFileName;
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				ok = ok && WalkAdd(zip, packRoot, child);
			else
				ok = ok && AddFileToZip(zip, packRoot, child);
		} while (ok && FindNextFileW(find, &fd));
		FindClose(find);
		return ok;
	}

	std::wstring TacoOutPath()
	{
		std::wstring p = AddonPaths::EnsureUnder(AddonPaths::DataDir(), L"pathing");
		p.push_back(L'\\');
		for (const char* c = TrailToolsDetail::gDraft.packName; *c; ++c)
			p.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*c)));
		p += L".taco";
		return p;
	}
}

bool TrailToolsBuild::BuildTaco(std::string& errOut)
{
	errOut.clear();
	using namespace TrailToolsDetail;
	SanitizePackName(gDraft.packName, sizeof(gDraft.packName));
	if (!EnsureWorkspace())
	{
		errOut = "Could not create authoring workspace.";
		return false;
	}
	if (gDraft.pois.empty() && gDraft.active.points.size() < 2 && gDraft.trails.empty())
	{
		errOut = "Nothing to build — drop markers or record a trail first.";
		return false;
	}

	/* Sync root category names from pack fields. */
	gDraft.root.name = RootCategoryName();
	if (gDraft.displayName[0])
		gDraft.root.displayName = gDraft.displayName;

	if (gDraft.active.points.size() >= 2 && gDraft.active.mapId != 0)
	{
		if (gDraft.active.fileRel.empty())
		{
			gDraft.active.fileRel = std::string("Data/") + gDraft.packName + "/Trails/" +
				(gDraft.trailFileStem[0] ? gDraft.trailFileStem : "Trail") + ".trl";
		}
		if (gDraft.active.type.empty() && gDraft.trailType[0])
			gDraft.active.type = gDraft.trailType;
		/* Upsert into trails list before flushing all .trl files. */
		bool found = false;
		for (auto& t : gDraft.trails)
		{
			if (t.fileRel == gDraft.active.fileRel)
			{
				t = gDraft.active;
				found = true;
				break;
			}
		}
		if (!found)
			gDraft.trails.push_back(gDraft.active);
	}

	for (const auto& t : gDraft.trails)
	{
		if (t.points.size() < 2 || t.mapId == 0 || t.fileRel.empty())
			continue;
		const std::wstring trlPath = RelToPackPath(t.fileRel);
		EnsureParentDirs(trlPath);
		if (!TrailToolsTrl::Write(trlPath, t.mapId, t.points))
		{
			errOut = "Failed to write .trl: " + t.fileRel;
			return false;
		}
	}

	if (!TrailToolsXml::WritePackXmlLayout(gDraft))
	{
		errOut = "Failed to write pack XML.";
		return false;
	}

	const std::wstring tacoW = TacoOutPath();
	DeleteFileW(tacoW.c_str());

	mz_zip_archive zip{};
	memset(&zip, 0, sizeof(zip));
	if (!mz_zip_writer_init_heap(&zip, 0, 256 * 1024))
	{
		errOut = "Failed to create .taco zip.";
		return false;
	}
	const std::wstring packRoot = PackDir();
	const bool ok = WalkAdd(zip, packRoot, packRoot);
	void* outBuf = nullptr;
	size_t outSize = 0;
	if (!ok || !mz_zip_writer_finalize_heap_archive(&zip, &outBuf, &outSize) || !outBuf || outSize == 0)
	{
		mz_zip_writer_end(&zip);
		if (outBuf)
			mz_free(outBuf);
		errOut = "Failed while writing pack entries.";
		return false;
	}
	mz_zip_writer_end(&zip);

	HANDLE h = CreateFileW(tacoW.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
	{
		mz_free(outBuf);
		errOut = "Failed to write .taco file.";
		return false;
	}
	DWORD written = 0;
	const BOOL wrote = WriteFile(h, outBuf, static_cast<DWORD>(outSize), &written, nullptr);
	CloseHandle(h);
	mz_free(outBuf);
	if (!wrote || written != outSize)
	{
		DeleteFileW(tacoW.c_str());
		errOut = "Failed to write .taco file.";
		return false;
	}
	return true;
}
