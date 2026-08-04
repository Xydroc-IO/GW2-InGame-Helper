#pragma once

#include <string>
#include <vector>

#include <windows.h>

/* Shared state / helpers for NotesPad.cpp + NotesPadWaypoints.cpp. */
namespace NotesPadDetail
{
	constexpr int kMaxSnippets = 48;
	constexpr int kTitleLen = 64;
	constexpr int kBodyLen = 512;
	constexpr float kNotesPadW = 500.f;
	constexpr float kNotesPadH = 640.f;
	/* Title + kind + multiline body + Delete/Copy — keep visible without resize. */
	constexpr float kEditorReserve = 230.f;

	extern bool gRequestDock;
	extern int gPadTab; /* 0 snippets, 1 waypoints */
	extern int gWpMode; /* 0 search, 1 by map, 2 this map */
	extern char gWpQuery[128];
	extern char gWpMapFilter[128];
	extern int gWpMapId;
	extern std::string gWpMapName;
	extern bool gWpWaypointsOnly;
	extern char gWpCopied[96];

	enum Kind : int
	{
		Kind_Waypoint = 0,
		Kind_Chat,
		Kind_Build,
		Kind_Lfg,
		Kind_Note,
		Kind_Count
	};

	struct Snippet
	{
		char title[kTitleLen]{};
		char body[kBodyLen]{};
		int kind = Kind_Note;
	};

	extern std::vector<Snippet> gSnips;
	extern bool gDirty;
	extern bool gLoaded;
	extern int gSelected;
	extern DWORD gLastSaveMs;

	inline const char* KindLabel(int k)
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

	std::wstring NotesPathW();
	void CopyText(const char* text);
	void MarkDirty();
	void DrawWaypointsTab();
}
