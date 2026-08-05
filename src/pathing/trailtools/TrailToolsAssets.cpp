#include "TrailToolsAssets.h"

#include "AddonPaths.h"
#include "PadNav.h"
#include "PathingParse.h"
#include "TrailToolsShared.h"
#include "HelperTheme.h"

#include "imgui/imgui.h"
#include "miniz/miniz.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace
{
	bool EndsWithPng(const std::string& s)
	{
		if (s.size() < 4)
			return false;
		const char* e = s.c_str() + s.size() - 4;
		return (e[0] == '.' || e[0] == '.') &&
			(e[1] == 'p' || e[1] == 'P') &&
			(e[2] == 'n' || e[2] == 'N') &&
			(e[3] == 'g' || e[3] == 'G');
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

	std::wstring MarkersDir()
	{
		std::wstring p = TrailToolsDetail::PackDir();
		p += L"\\Data\\";
		for (const char* c = TrailToolsDetail::gDraft.packName; *c; ++c)
			p.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*c)));
		p += L"\\Markers";
		return p;
	}

	bool WriteBytesW(const std::wstring& path, const void* data, size_t len)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, data, static_cast<DWORD>(len), &written, nullptr);
		CloseHandle(h);
		return ok && written == len;
	}

	std::string FileNameOf(const std::string& path)
	{
		const size_t slash = path.find_last_of("/\\");
		return slash == std::string::npos ? path : path.substr(slash + 1);
	}
}

void TrailToolsAssets::RefreshAuthoringList(std::vector<Entry>& out)
{
	out.clear();
	TrailToolsDetail::EnsureWorkspace();
	const std::wstring dir = MarkersDir();
	const std::wstring glob = dir + L"\\*";
	WIN32_FIND_DATAW fd{};
	HANDLE h = FindFirstFileW(glob.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE)
		return;
	const std::string prefix = std::string("Data/") + TrailToolsDetail::gDraft.packName + "/Markers/";
	do
	{
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		const std::string name = WideToUtf8(fd.cFileName);
		if (!EndsWithPng(name))
			continue;
		Entry e;
		e.relPath = prefix + name;
		e.label = name;
		e.fromTaco = false;
		out.push_back(std::move(e));
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	std::sort(out.begin(), out.end(),
		[](const Entry& a, const Entry& b) { return a.label < b.label; });
}

void TrailToolsAssets::RefreshInstalledTacoList(std::vector<Entry>& out)
{
	out.clear();
	const std::wstring root = AddonPaths::EnsureUnder(AddonPaths::DataDir(), L"pathing");
	const std::wstring glob = root + L"\\*.taco";
	WIN32_FIND_DATAW fd{};
	HANDLE h = FindFirstFileW(glob.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE)
		return;
	do
	{
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		const std::wstring tacoPath = root + L"\\" + fd.cFileName;
		const std::string tacoName = WideToUtf8(fd.cFileName);
		mz_zip_archive zip{};
		memset(&zip, 0, sizeof(zip));
		std::vector<uint8_t> file;
		if (!PathingParse::ReadFileW(tacoPath, file, PathingParse::kMaxZipBytes))
			continue;
		if (!mz_zip_reader_init_mem(&zip, file.data(), file.size(), 0))
			continue;
		const mz_uint n = mz_zip_reader_get_num_files(&zip);
		for (mz_uint i = 0; i < n && out.size() < 400; ++i)
		{
			mz_zip_archive_file_stat st{};
			if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory)
				continue;
			std::string entry = st.m_filename ? st.m_filename : "";
			std::replace(entry.begin(), entry.end(), '\\', '/');
			if (!EndsWithPng(entry))
				continue;
			/* Prefer trail/marker-ish paths to keep list useful. */
			std::string low = PathingParse::ToLower(entry);
			const bool interesting =
				low.find("trail") != std::string::npos ||
				low.find("marker") != std::string::npos ||
				low.find("/images/") != std::string::npos ||
				low.find("poi") != std::string::npos ||
				low.find("chevron") != std::string::npos;
			if (!interesting)
				continue;
			Entry e;
			e.relPath = entry;
			e.label = tacoName + " · " + FileNameOf(entry);
			e.fromTaco = true;
			e.tacoName = tacoName;
			e.zipEntry = entry;
			out.push_back(std::move(e));
		}
		mz_zip_reader_end(&zip);
	} while (FindNextFileW(h, &fd));
	FindClose(h);
}

bool TrailToolsAssets::ImportFromTaco(const std::wstring& tacoPath,
	const std::string& zipEntry, std::string& outRelPath, std::string& err)
{
	TrailToolsDetail::EnsureWorkspace();
	std::vector<uint8_t> bytes;
	if (!PathingParse::ZipReadEntry(tacoPath, zipEntry, bytes, 4u * 1024u * 1024u) ||
		bytes.empty())
	{
		err = "Could not extract texture from .taco.";
		return false;
	}
	const std::string fname = FileNameOf(zipEntry);
	if (fname.empty())
	{
		err = "Bad texture name.";
		return false;
	}
	CreateDirectoryW(MarkersDir().c_str(), nullptr);
	const std::wstring dest = MarkersDir() + L"\\" + Utf8ToWide(fname);
	if (!WriteBytesW(dest, bytes.data(), bytes.size()))
	{
		err = "Failed to write into Markers/.";
		return false;
	}
	outRelPath = std::string("Data/") + TrailToolsDetail::gDraft.packName +
		"/Markers/" + fname;
	return true;
}

void TrailToolsAssets::DrawBrowserUi()
{
	using namespace TrailToolsDetail;
	static std::vector<Entry> sAuth;
	static std::vector<Entry> sTaco;
	static bool sLoaded = false;
	if (!sLoaded)
	{
		RefreshAuthoringList(sAuth);
		RefreshInstalledTacoList(sTaco);
		sLoaded = true;
	}

	ImGui::TextUnformatted("Texture browser");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Pick a PNG from your pack or import one from an installed .taco.");
	PadNav::PopWrap();

	if (ImGui::Button("Refresh lists###gw2igh_tt_texref"))
	{
		RefreshAuthoringList(sAuth);
		RefreshInstalledTacoList(sTaco);
	}

	if (ImGui::BeginChild("###gw2igh_tt_texauth", ImVec2(0.f, 110.f), true))
	{
		ImGui::TextDisabled("Authoring Markers/ (%zu)", sAuth.size());
		for (size_t i = 0; i < sAuth.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			if (ImGui::Selectable(sAuth[i].label.c_str()))
			{
				CategoryNode* trail = FindCategoryByPath(gDraft.root,
					gDraft.trailType[0] ? std::string(gDraft.trailType)
						: RootCategoryName() + ".t.extrail");
				CategoryNode* mark = FindCategoryByPath(gDraft.root,
					gDraft.markerType[0] ? std::string(gDraft.markerType)
						: RootCategoryName() + ".m.exm");
				if (trail && (sAuth[i].label.find("Trail") != std::string::npos ||
					sAuth[i].label.find("trail") != std::string::npos))
					trail->texture = sAuth[i].relPath;
				else if (mark)
					mark->iconFile = sAuth[i].relPath;
				else if (trail)
					trail->texture = sAuth[i].relPath;
				SetStatus("Selected %s", sAuth[i].relPath.c_str());
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();

	if (ImGui::BeginChild("###gw2igh_tt_textaco", ImVec2(0.f, 120.f), true))
	{
		ImGui::TextDisabled("Installed .taco textures (%zu shown)", sTaco.size());
		for (size_t i = 0; i < sTaco.size(); ++i)
		{
			ImGui::PushID(10000 + static_cast<int>(i));
			if (ImGui::Selectable(sTaco[i].label.c_str()))
			{
				std::wstring taco = AddonPaths::EnsureUnder(AddonPaths::DataDir(), L"pathing");
				taco += L"\\";
				taco += Utf8ToWide(sTaco[i].tacoName);
				std::string rel, err;
				if (!ImportFromTaco(taco, sTaco[i].zipEntry, rel, err))
					SetStatus("%s", err.c_str());
				else
				{
					RefreshAuthoringList(sAuth);
					CategoryNode* trail = FindCategoryByPath(gDraft.root,
						gDraft.trailType[0] ? std::string(gDraft.trailType)
							: RootCategoryName() + ".t.extrail");
					CategoryNode* mark = FindCategoryByPath(gDraft.root,
						gDraft.markerType[0] ? std::string(gDraft.markerType)
							: RootCategoryName() + ".m.exm");
					const std::string low = PathingParse::ToLower(rel);
					if (trail && low.find("trail") != std::string::npos)
						trail->texture = rel;
					else if (mark)
						mark->iconFile = rel;
					else if (trail)
						trail->texture = rel;
					SetStatus("Imported %s", rel.c_str());
				}
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();
}
