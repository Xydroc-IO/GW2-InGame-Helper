#include "NotesPad.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "PadDock.h"
#include "Settings.h"
#include "WaypointsData.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace
{
	constexpr int kMaxSnippets = 48;
	constexpr int kTitleLen = 64;
	constexpr int kBodyLen = 512;
	constexpr float kNotesPadW = 440.f;
	constexpr float kNotesPadH = 600.f;
	/* Title + kind + multiline body + Delete/Copy — keep visible without resize. */
	constexpr float kEditorReserve = 230.f;

	bool gRequestDock = false;
	int gPadTab = 0; /* 0 snippets, 1 waypoints */
	int gWpMode = 0; /* 0 search, 1 by map, 2 this map */
	char gWpQuery[128] = {};
	char gWpMapFilter[128] = {};
	int gWpMapId = 0;
	std::string gWpMapName;
	bool gWpWaypointsOnly = true;
	char gWpCopied[96] = {};

	enum Kind : int
	{
		Kind_Waypoint = 0,
		Kind_Chat,
		Kind_Build,
		Kind_Lfg,
		Kind_Note,
		Kind_Count
	};

	const char* KindLabel(int k)
	{
		switch (k)
		{
		case Kind_Waypoint: return "Waypoint";
		case Kind_Chat: return "Chat";
		case Kind_Build: return "Build";
		case Kind_Lfg: return "LFG";
		default: return "Note";
		}
	}

	struct Snippet
	{
		char title[kTitleLen]{};
		char body[kBodyLen]{};
		int kind = Kind_Note;
	};

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

	void CopyText(const char* text)
	{
		if (!text || !text[0])
			return;
		if (!OpenClipboard(nullptr))
			return;
		EmptyClipboard();
		const SIZE_T bytes = std::strlen(text) + 1;
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
		if (mem)
		{
			void* locked = GlobalLock(mem);
			if (locked)
			{
				std::memcpy(locked, text, bytes);
				GlobalUnlock(mem);
				SetClipboardData(CF_TEXT, mem);
			}
		}
		CloseClipboard();
	}

	void MarkDirty()
	{
		gDirty = true;
	}

	void DrawWaypointsTab()
	{
		WaypointsData::EnsureLoaded(false);
		WaypointsData::Tick();

		ImGui::PushTextWrapPos(0.f);
		ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
			"Official API waypoints & POIs — Copy puts the chat code on your clipboard.");
		ImGui::PopTextWrapPos();

		if (WaypointsData::Busy())
			ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f), "%s", WaypointsData::Status());
		else if (!WaypointsData::Ready())
		{
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "%s", WaypointsData::Status());
			if (ImGui::Button("Load waypoints###gw2igh_wp_load"))
				WaypointsData::EnsureLoaded(true);
			return;
		}
		else
			ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f), "%s", WaypointsData::Status());

		ImGui::Checkbox("Waypoints only###gw2igh_wp_wponly", &gWpWaypointsOnly);
		ImGui::SameLine();
		if (ImGui::SmallButton("Reload###gw2igh_wp_reload"))
			WaypointsData::EnsureLoaded(true);

		if (ImGui::RadioButton("Search###gw2igh_wp_m0", gWpMode == 0)) gWpMode = 0;
		ImGui::SameLine();
		if (ImGui::RadioButton("By map###gw2igh_wp_m1", gWpMode == 1)) gWpMode = 1;
		ImGui::SameLine();
		if (ImGui::RadioButton("This map###gw2igh_wp_m2", gWpMode == 2)) gWpMode = 2;

		std::vector<WaypointsData::Poi> hits;
		std::vector<WaypointsData::MapRow> maps;

		if (gWpMode == 0)
		{
			ImGui::SetNextItemWidth(-1.f);
			ImGui::InputTextWithHint("###gw2igh_wp_q", "Waypoint or map name…",
				gWpQuery, sizeof(gWpQuery));
			if (gWpQuery[0])
				WaypointsData::Search(gWpQuery, gWpWaypointsOnly, hits, 100);
			else
				ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
					"Type a name — e.g. fort trinity, lion's arch.");
		}
		else if (gWpMode == 1)
		{
			ImGui::SetNextItemWidth(-1.f);
			ImGui::InputTextWithHint("###gw2igh_wp_mf", "Filter maps…",
				gWpMapFilter, sizeof(gWpMapFilter));
			WaypointsData::ListMaps(gWpMapFilter, maps, 60);
			const float mapListH = 120.f;
			ImGui::BeginChild("###gw2igh_wp_maps", ImVec2(0.f, mapListH), true);
			for (const WaypointsData::MapRow& m : maps)
			{
				ImGui::PushID(m.id);
				char label[160];
				std::snprintf(label, sizeof(label), "%s  (%d wp)",
					m.name.c_str(), m.waypointCount);
				if (ImGui::Selectable(label, gWpMapId == m.id))
				{
					gWpMapId = m.id;
					gWpMapName = m.name;
				}
				ImGui::PopID();
			}
			ImGui::EndChild();
			if (gWpMapId > 0)
			{
				ImGui::TextColored(ImVec4(0.85f, 0.80f, 0.95f, 1.f), "%s",
					gWpMapName.empty() ? "Map" : gWpMapName.c_str());
				WaypointsData::ListForMap(gWpMapId, gWpWaypointsOnly, hits);
			}
		}
		else
		{
			const int cur = WaypointsData::CurrentMapId();
			if (cur <= 0)
			{
				ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.4f, 1.f),
					"Waiting for MumbleLink / enter the world.");
			}
			else
			{
				gWpMapId = cur;
				WaypointsData::ListForMap(cur, gWpWaypointsOnly, hits);
				const char* mapLabel = hits.empty() ? "This map" : hits[0].mapName.c_str();
				ImGui::TextColored(ImVec4(0.85f, 0.80f, 0.95f, 1.f),
					"%s  (#%d)", mapLabel, cur);
				if (hits.empty())
					ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f),
						"No waypoints indexed for this map id.");
			}
		}

		if (gWpCopied[0])
		{
			ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.55f, 1.f),
				"Copied %s", gWpCopied);
		}

		const float listH = ImGui::GetContentRegionAvail().y;
		ImGui::BeginChild("###gw2igh_wp_list", ImVec2(0.f, listH > 80.f ? listH : 80.f), true);
		for (size_t i = 0; i < hits.size(); ++i)
		{
			const WaypointsData::Poi& p = hits[i];
			ImGui::PushID(static_cast<int>(p.id));
			ImGui::TextUnformatted(p.name.c_str());
			if (gWpMode == 0)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.50f, 0.52f, 0.56f, 1.f), "· %s", p.mapName.c_str());
			}
			if (!gWpWaypointsOnly && p.type != "waypoint")
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.60f, 0.55f, 0.45f, 1.f), "[%s]", p.type.c_str());
			}
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "%s", p.chatLink.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton("Copy"))
			{
				CopyText(p.chatLink.c_str());
				std::snprintf(gWpCopied, sizeof(gWpCopied), "%s", p.chatLink.c_str());
			}
			ImGui::PopID();
		}
		if (hits.empty() && ((gWpMode == 0 && gWpQuery[0]) ||
				(gWpMode == 1 && gWpMapId > 0) ||
				(gWpMode == 2 && gWpMapId > 0)))
		{
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.f), "No matches.");
		}
		ImGui::EndChild();
	}

	void DrawSnippetsTab()
	{
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
			ImGui::SetNextItemWidth(160.f);
			if (ImGui::Combo("###gw2igh_notes_kind", &s.kind, kinds, Kind_Count))
				MarkDirty();
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
		/* bodies may contain } — match braces with string awareness */
		size_t nextObj = raw.find("{\"title\"", brace + 1);
		if (nextObj == std::string::npos)
			nextObj = raw.find("\n  {", brace + 1);
		size_t chunkEnd = (nextObj != std::string::npos) ? nextObj : raw.size();
		/* Prefer matching braces naively for short objects */
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
	gRequestDock = true;
	Settings::SetDirty();
}

bool NotesPad::Render()
{
	if (!G::ShowNotes)
	{
		PadDock::ClearNotes();
		return false;
	}
	if (!gLoaded)
		Load();

	ImGui::SetNextWindowSizeConstraints(ImVec2(380.f, 460.f), ImVec2(720.f, 1200.f));
	/* Appearing (every open this session) — old imgui.ini sizes were too short
	   and hid the body text box until the user resized. */
	ImGui::SetNextWindowSize(ImVec2(kNotesPadW, kNotesPadH), ImGuiCond_Appearing);
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	if (gRequestDock)
	{
		ImGui::SetNextWindowPos(PadDock::ForNotes(kNotesPadW), ImGuiCond_Always);
		ImGui::SetNextWindowFocus();
		gRequestDock = false;
	}
	bool open = G::ShowNotes;
	if (!ImGui::Begin("Notes & Waypoints##GW2InGameHelperNotes", &open))
	{
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
	PadDock::RememberNotes(ImGui::GetWindowPos(), ImGui::GetWindowSize());

	ImGui::TextUnformatted("Notes & Waypoints");
	ImGui::TextColored(ImVec4(0.66f, 0.68f, 0.72f, 1.f),
		"Local clipboard helpers + official map waypoints. No game injection.");
	ImGui::Separator();

	if (ImGui::BeginTabBar("###gw2igh_notes_tabs"))
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
