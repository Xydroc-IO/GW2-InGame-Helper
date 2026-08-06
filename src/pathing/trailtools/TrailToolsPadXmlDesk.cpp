#include "TrailToolsInternal.h"
#include "TrailToolsShared.h"
#include "TrailToolsXml.h"

#include "HelperTheme.h"
#include "PadNav.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>

#include <windows.h>
#include <commdlg.h>

namespace
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

	std::wstring DefaultXmlPath()
	{
		using namespace TrailToolsDetail;
		EnsureWorkspace();
		std::wstring p = PackDir();
		p.push_back(L'\\');
		for (const char* c = gDraft.packName; *c; ++c)
			p.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*c)));
		p += (gDraft.xmlLayout == 1) ? L"_Data.xml" : L".xml";
		return p;
	}

	bool DialogPickXml(bool save, std::wstring& outPath)
	{
		using namespace TrailToolsDetail;
		EnsureWorkspace();
		wchar_t file[MAX_PATH]{};
		OPENFILENAMEW ofn{};
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFilter = L"Overlay XML (*.xml)\0*.xml\0All\0*.*\0";
		ofn.lpstrFile = file;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
		ofn.lpstrDefExt = L"xml";
		std::wstring dir = PackDir();
		ofn.lpstrInitialDir = dir.c_str();
		const BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
		if (!ok)
			return false;
		outPath = file;
		return true;
	}

	bool ReadAllUtf8(const std::wstring& path, std::string& out)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 8 * 1024 * 1024)
		{
			CloseHandle(h);
			return false;
		}
		out.assign(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD rd = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &rd, nullptr);
		CloseHandle(h);
		if (!ok)
			return false;
		out.resize(rd);
		return true;
	}
}

namespace TrailToolsDetail
{
	void UpsertActiveTrailInPack()
	{
		if (gDraft.active.fileRel.empty())
		{
			SetStatus("Active trail has no file path - set stem or Save .trl first.");
			return;
		}
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
		gDraft.xmlDirty = true;
		gDraft.trailDirty = false;
		SetStatus("Inserted trail into project (%s).", gDraft.active.fileRel.c_str());
	}

	void UpsertSelectedPoiInPack()
	{
		if (gDraft.selectedPoi < 0 ||
			gDraft.selectedPoi >= static_cast<int>(gDraft.pois.size()))
		{
			SetStatus("Select a marker first.");
			return;
		}
		gDraft.xmlDirty = true;
		SetStatus("Marker %d is in the project (save XML to write disk).", gDraft.selectedPoi);
	}

	void NewProjectXml()
	{
		EnsureWorkspace();
		const std::wstring def = DefaultXmlPath();
		std::snprintf(gDraft.xmlPath, sizeof(gDraft.xmlPath), "%s", WideToUtf8(def).c_str());
		gDraft.xmlDirty = true;
		SetStatus("New project path set - Save XML to write OverlayData.");
		Settings::SetDirty();
	}

	bool SaveProjectXml(bool saveAs)
	{
		EnsureWorkspace();
		std::wstring path;
		if (saveAs || !gDraft.xmlPath[0])
		{
			if (!DialogPickXml(true, path))
			{
				SetStatus("Save XML cancelled.");
				return false;
			}
			std::snprintf(gDraft.xmlPath, sizeof(gDraft.xmlPath), "%s", WideToUtf8(path).c_str());
		}
		else
			path = Utf8ToWide(gDraft.xmlPath);

		if (gDraft.xmlLayout == 1 && !saveAs)
		{
			if (!TrailToolsXml::WritePackXmlLayout(gDraft))
			{
				SetStatus("Save XML (split layout) failed.");
				return false;
			}
		}
		else if (!TrailToolsXml::WriteOverlayFile(path, gDraft))
		{
			SetStatus("Save XML failed.");
			return false;
		}
		gDraft.xmlDirty = false;
		SetStatus("Saved project XML.");
		Settings::SetDirty();
		return true;
	}

	bool LoadProjectXml()
	{
		std::wstring path;
		if (!DialogPickXml(false, path))
		{
			SetStatus("Load XML cancelled.");
			return false;
		}
		std::string xml;
		if (!ReadAllUtf8(path, xml))
		{
			SetStatus("Could not read XML.");
			return false;
		}
		/* Reuse draft-session parse by writing temp then LoadDraftSession is awkward —
		   call the same loader via a short path: write to session then load. */
		EnsureWorkspace();
		const std::wstring session = PackDir() + L"\\_draft_session.xml";
		if (!TrailToolsXml::WriteUtf8File(session, xml))
		{
			SetStatus("Failed to stage XML for load.");
			return false;
		}
		if (!LoadDraftSession())
			return false;
		std::snprintf(gDraft.xmlPath, sizeof(gDraft.xmlPath), "%s", WideToUtf8(path).c_str());
		gDraft.xmlDirty = false;
		SetStatus("Loaded project XML (%zu trails, %zu markers).",
			gDraft.trails.size(), gDraft.pois.size());
		Settings::SetDirty();
		return true;
	}

	void DrawXmlProjectDesk()
	{
		ImGui::TextUnformatted("Associated XML");
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Shared OverlayData project for Trails + Markers. Save writes categories + POIs "
			"(and Trail refs). .trl binaries are still Save'd from the Trails window.");
		PadNav::PopWrap();

		PadNav::PushWidthForLabel("XML path###gw2igh_tt_xmlpath");
		ImGui::InputText("XML path###gw2igh_tt_xmlpath", gDraft.xmlPath, sizeof(gDraft.xmlPath));
		PadNav::PopWidthForLabel();
		if (gDraft.xmlDirty)
			ImGui::SameLine();
		if (gDraft.xmlDirty)
			ImGui::TextColored(HelperTheme::Warn, "*");

		if (ImGui::Button("New###gw2igh_tt_xmlnew"))
			NewProjectXml();
		PadNav::WrapSameLine(PadNav::ButtonWidth("Load..."));
		if (ImGui::Button("Load...###gw2igh_tt_xmlload"))
			LoadProjectXml();
		PadNav::WrapSameLine(PadNav::ButtonWidth("Save"));
		if (ImGui::Button("Save###gw2igh_tt_xmlsave"))
			SaveProjectXml(false);
		PadNav::WrapSameLine(PadNav::ButtonWidth("Save As..."));
		if (ImGui::Button("Save As...###gw2igh_tt_xmlsaveas"))
			SaveProjectXml(true);

		ImGui::TextUnformatted("XML layout");
		if (ImGui::RadioButton("Combined###gw2igh_tt_xml_comb", gDraft.xmlLayout == 0))
		{
			gDraft.xmlLayout = 0;
			Settings::SetDirty();
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Split menu + data###gw2igh_tt_xml_split", gDraft.xmlLayout == 1))
		{
			gDraft.xmlLayout = 1;
			Settings::SetDirty();
		}
	}
}
