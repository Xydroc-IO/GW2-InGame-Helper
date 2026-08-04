#include "NotesPad.h"
#include "NotesPadInternal.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "PadDock.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace NotesPadDetail
{
	bool gRequestDock = false;
	int gPadTab = 0;
	int gWpMode = 0;
	char gWpQuery[128] = {};
	char gWpMapFilter[128] = {};
	int gWpMapId = 0;
	std::string gWpMapName;
	bool gWpWaypointsOnly = true;
	char gWpCopied[96] = {};

	std::vector<Snippet> gSnips;
	bool gDirty = false;
	bool gLoaded = false;
	int gSelected = -1;
	DWORD gLastSaveMs = 0;

	std::wstring NotesPathW()
	{
		return AddonPaths::DataDir() + L"\\notes.json";
	}

	std::string HtmlEscapeJson(const char* s)
	{
		std::string o;
		if (!s) return o;
		for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p)
		{
			char c = static_cast<char>(*p);
			if (c == '\\' || c == '"')
			{
				o.push_back('\\');
				o.push_back(c);
			}
			else if (c == '\n')
				o += "\\n";
			else if (c == '\r')
				continue;
			else if (c == '\t')
				o += "\\t";
			else if (static_cast<unsigned char>(c) < 0x20)
				continue;
			else
				o.push_back(c);
		}
		return o;
	}

	bool WriteUtf8File(const std::wstring& path, const std::string& data)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
		CloseHandle(h);
		return ok && written == data.size();
	}

	std::string ReadUtf8File(const std::wstring& path)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return {};
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 2 * 1024 * 1024)
		{
			CloseHandle(h);
			return {};
		}
		std::string out(static_cast<size_t>(sz.QuadPart), '\0');
		DWORD read = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
		CloseHandle(h);
		if (!ok)
			return {};
		out.resize(read);
		return out;
	}

	std::string JsonStringAfterKey(const std::string& json, const char* key, size_t from)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos)
			return {};
		k = json.find(':', k + pat.size());
		if (k == std::string::npos)
			return {};
		++k;
		while (k < json.size() && (json[k] == ' ' || json[k] == '\t' || json[k] == '\n' || json[k] == '\r'))
			++k;
		if (k >= json.size() || json[k] != '"')
			return {};
		++k;
		std::string out;
		while (k < json.size())
		{
			char c = json[k++];
			if (c == '\\' && k < json.size())
			{
				char e = json[k++];
				if (e == 'n') out.push_back('\n');
				else if (e == 't') out.push_back('\t');
				else out.push_back(e);
				continue;
			}
			if (c == '"')
				break;
			out.push_back(c);
		}
		return out;
	}

	long long JsonIntAfterKey(const std::string& json, const char* key, size_t from)
	{
		std::string pat = "\"";
		pat += key;
		pat += "\"";
		size_t k = json.find(pat, from);
		if (k == std::string::npos)
			return -1;
		k = json.find(':', k + pat.size());
		if (k == std::string::npos)
			return -1;
		++k;
		while (k < json.size() && (json[k] == ' ' || json[k] == '\t'))
			++k;
		bool neg = false;
		if (k < json.size() && json[k] == '-')
		{
			neg = true;
			++k;
		}
		long long v = 0;
		bool any = false;
		while (k < json.size() && json[k] >= '0' && json[k] <= '9')
		{
			any = true;
			v = v * 10 + (json[k] - '0');
			++k;
		}
		if (!any)
			return -1;
		return neg ? -v : v;
	}

	void SeedDefaults()
	{
		gSnips.clear();
		Snippet a{};
		std::snprintf(a.title, sizeof(a.title), "LFG — Raids");
		std::snprintf(a.body, sizeof(a.body),
			"lfg w1–4 exp | [roles] | [kp] | discord: ");
		a.kind = Kind_Lfg;
		gSnips.push_back(a);

		Snippet b{};
		std::snprintf(b.title, sizeof(b.title), "Waypoint");
		std::snprintf(b.body, sizeof(b.body), "[&AAAAAAA=]");
		b.kind = Kind_Waypoint;
		gSnips.push_back(b);

		Snippet c{};
		std::snprintf(c.title, sizeof(c.title), "Build code");
		std::snprintf(c.body, sizeof(c.body), "[&AAAAAAA=]");
		c.kind = Kind_Build;
		gSnips.push_back(c);
	}

	void MarkDirty()
	{
		gDirty = true;
	}
}

namespace
{
	void DrawSnippetsTab()
	{
		using namespace NotesPadDetail;

		if (ImGui::Button("Add snippet###gw2igh_notes_add") &&
			static_cast<int>(gSnips.size()) < kMaxSnippets)
		{
			Snippet s{};
			std::snprintf(s.title, sizeof(s.title), "New note");
			s.kind = Kind_Note;
			gSnips.push_back(s);
			gSelected = static_cast<int>(gSnips.size()) - 1;
			MarkDirty();
		}
		ImGui::SameLine();
		if (ImGui::Button("Copy selected###gw2igh_notes_copy") &&
			gSelected >= 0 && gSelected < static_cast<int>(gSnips.size()))
		{
			CopyText(gSnips[static_cast<size_t>(gSelected)].body);
		}

		float listH = ImGui::GetContentRegionAvail().y - kEditorReserve;
		if (listH < 72.f)
			listH = 72.f;
		ImGui::BeginChild("###gw2igh_notes_list", ImVec2(0.f, listH), true);
		for (int i = 0; i < static_cast<int>(gSnips.size()); ++i)
		{
			Snippet& s = gSnips[static_cast<size_t>(i)];
			ImGui::PushID(i);
			char label[96];
			std::snprintf(label, sizeof(label), "[%s] %s", KindLabel(s.kind),
				s.title[0] ? s.title : "(untitled)");
			if (ImGui::Selectable(label, gSelected == i))
				gSelected = i;
			ImGui::SameLine();
			if (ImGui::SmallButton("Copy"))
				CopyText(s.body);
			ImGui::PopID();
		}
		ImGui::EndChild();

		if (gSelected >= 0 && gSelected < static_cast<int>(gSnips.size()))
		{
			Snippet& s = gSnips[static_cast<size_t>(gSelected)];
			ImGui::Separator();
			ImGui::SetNextItemWidth(-1.f);
			if (ImGui::InputTextWithHint("###gw2igh_notes_title", "Title", s.title, sizeof(s.title)))
				MarkDirty();
			const char* kinds[] = { "Waypoint", "Chat", "Build", "LFG", "Note" };
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "Kind");
			for (int ki = 0; ki < Kind_Count; ++ki)
			{
				if (ki > 0)
					ImGui::SameLine();
				char id[48];
				std::snprintf(id, sizeof(id), "%s###gw2igh_notes_kind%d", kinds[ki], ki);
				if (ImGui::RadioButton(id, &s.kind, ki))
					MarkDirty();
			}
			ImGui::SetNextItemWidth(-1.f);
			const float availBody = ImGui::GetContentRegionAvail().y - 36.f;
			const float bodyH = (availBody > 100.f) ? availBody : 100.f;
			if (ImGui::InputTextMultiline("###gw2igh_notes_body", s.body, sizeof(s.body),
					ImVec2(-1.f, bodyH)))
				MarkDirty();
			if (ImGui::Button("Delete###gw2igh_notes_del"))
			{
				gSnips.erase(gSnips.begin() + gSelected);
				if (gSelected >= static_cast<int>(gSnips.size()))
					gSelected = static_cast<int>(gSnips.size()) - 1;
				MarkDirty();
			}
			ImGui::SameLine();
			if (ImGui::Button("Copy body###gw2igh_notes_copy2"))
				CopyText(s.body);
		}
		else
		{
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
				"Select a snippet or Add one to edit the text box below the list.");
		}
	}
}

void NotesPad::Load()
{
	using namespace NotesPadDetail;

	gLoaded = true;
	gSnips.clear();
	gSelected = -1;
	const std::string raw = ReadUtf8File(NotesPathW());
	if (raw.empty())
	{
		SeedDefaults();
		gDirty = true;
		if (!gSnips.empty())
			gSelected = 0;
		return;
	}

	size_t p = 0;
	while (p < raw.size() && gSnips.size() < static_cast<size_t>(kMaxSnippets))
	{
		size_t brace = raw.find('{', p);
		if (brace == std::string::npos)
			break;
		size_t nextObj = raw.find("{\"title\"", brace + 1);
		if (nextObj == std::string::npos)
			nextObj = raw.find("\n  {", brace + 1);
		size_t chunkEnd = (nextObj != std::string::npos) ? nextObj : raw.size();
		int depth = 0;
		bool inStr = false;
		bool esc = false;
		size_t realEnd = std::string::npos;
		for (size_t i = brace; i < chunkEnd; ++i)
		{
			char c = raw[i];
			if (inStr)
			{
				if (esc) esc = false;
				else if (c == '\\') esc = true;
				else if (c == '"') inStr = false;
				continue;
			}
			if (c == '"') inStr = true;
			else if (c == '{') ++depth;
			else if (c == '}')
			{
				--depth;
				if (depth == 0)
				{
					realEnd = i;
					break;
				}
			}
		}
		if (realEnd == std::string::npos)
		{
			p = brace + 1;
			continue;
		}

		std::string title = JsonStringAfterKey(raw, "title", brace);
		std::string body = JsonStringAfterKey(raw, "body", brace);
		long long kind = JsonIntAfterKey(raw, "kind", brace);
		if (kind < 0 || kind >= Kind_Count)
			kind = Kind_Note;

		Snippet s{};
		std::snprintf(s.title, sizeof(s.title), "%s", title.c_str());
		std::snprintf(s.body, sizeof(s.body), "%s", body.c_str());
		s.kind = static_cast<int>(kind);
		gSnips.push_back(s);
		p = realEnd + 1;
	}

	if (gSnips.empty())
		SeedDefaults();
	if (!gSnips.empty())
		gSelected = 0;
	gDirty = false;
}

void NotesPad::Save(bool force)
{
	using namespace NotesPadDetail;

	if (!gLoaded || !gDirty)
		return;
	const DWORD now = GetTickCount();
	if (!force && gLastSaveMs != 0 && (now - gLastSaveMs) < 2500u)
		return;

	AddonPaths::DataDir();
	std::string out = "{\n  \"snippets\": [\n";
	for (size_t i = 0; i < gSnips.size(); ++i)
	{
		const Snippet& s = gSnips[i];
		out += "    {\"title\":\"";
		out += HtmlEscapeJson(s.title);
		out += "\",\"kind\":";
		out += std::to_string(s.kind);
		out += ",\"body\":\"";
		out += HtmlEscapeJson(s.body);
		out += "\"}";
		if (i + 1 < gSnips.size())
			out += ',';
		out += '\n';
	}
	out += "  ]\n}\n";
	if (WriteUtf8File(NotesPathW(), out))
	{
		gDirty = false;
		gLastSaveMs = now;
	}
}

void NotesPad::Open()
{
	G::ShowNotes = true;
	NotesPadDetail::gRequestDock = true;
	Settings::SetDirty();
}

bool NotesPad::Render()
{
	using namespace NotesPadDetail;

	if (!G::ShowNotes)
	{
		PadDock::ClearNotes();
		return false;
	}
	if (!gLoaded)
		Load();

	ImGui::SetNextWindowSizeConstraints(ImVec2(380.f, 400.f),
		ImVec2(PadDock::MaxW(720.f), PadDock::MaxH(460.f)));
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	PadDock::Place(G::PadNotes, gRequestDock, kNotesPadW, kNotesPadH, PadDock::ForNotes(kNotesPadW));
	if (!gRequestDock && G::PadNotes.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kNotesPadW, kNotesPadH), ImGuiCond_Appearing);
	bool open = G::ShowNotes;
	HelperTheme::ScopedWindow theme(G::Opacity);
	if (!ImGui::Begin("Notes & Waypoints##GW2InGameHelperNotes", &open))
	{
		if (PadDock::Capture(G::PadNotes))
			Settings::SetDirty();
		PadDock::RememberNotes(ImGui::GetWindowPos(), ImGui::GetWindowSize());
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
		ImGui::End();
		if (!open)
		{
			G::ShowNotes = false;
			PadDock::ClearNotes();
			Settings::SetDirty();
		}
		NotesPad::Save(false);
		return hovered;
	}
	if (!open)
	{
		G::ShowNotes = false;
		PadDock::ClearNotes();
		Settings::SetDirty();
	}
	if (PadDock::Capture(G::PadNotes))
		Settings::SetDirty();
	PadDock::RememberNotes(ImGui::GetWindowPos(), ImGui::GetWindowSize());

	HelperTheme::ScopedFontScale fontScale;

	ImGui::TextColored(HelperTheme::Gold, "NOTES & WAYPOINTS");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Local clipboard helpers + official map waypoints. No game injection.");
	PadNav::PopWrap();
	ImGui::Separator();

	if (ImGui::BeginTabBar("###gw2igh_notes_tabs", ImGuiTabBarFlags_FittingPolicyScroll))
	{
		if (ImGui::BeginTabItem("Snippets###gw2igh_notes_tab_snip"))
		{
			gPadTab = 0;
			DrawSnippetsTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Waypoints###gw2igh_notes_tab_wp"))
		{
			gPadTab = 1;
			DrawWaypointsTab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
	ImGui::End();
	NotesPad::Save(false);
	return hovered;
}
