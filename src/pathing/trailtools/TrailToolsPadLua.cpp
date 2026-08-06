#include "TrailToolsShared.h"

#include "AddonPaths.h"
#include "HelperTheme.h"
#include "PadNav.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <string>
#include <vector>

#include <windows.h>

/* List / note .lua files under authoring workspace for pack build inclusion. */
namespace TrailToolsDetail
{
	void DrawLuaFilesUi()
	{
		EnsureWorkspace();
		ImGui::TextUnformatted("Lua scripts");
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Drop .lua files into the pack authoring folder (any subpath). "
			"Build .taco includes them; enable Lua under Pathing -> Features to run.");
		PadNav::PopWrap();

		std::vector<std::string> luas;
		const std::wstring root = PackDir();
		/* Shallow scan of root + Scripts/ + Data/ */
		auto scan = [&](const std::wstring& dir, const std::string& prefix) {
			const std::wstring glob = dir + L"\\*.lua";
			WIN32_FIND_DATAW fd{};
			HANDLE h = FindFirstFileW(glob.c_str(), &fd);
			if (h == INVALID_HANDLE_VALUE)
				return;
			do
			{
				if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					continue;
				char name[260]{};
				WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, name, sizeof(name), nullptr, nullptr);
				luas.push_back(prefix + name);
			} while (FindNextFileW(h, &fd));
			FindClose(h);
		};
		scan(root, "");
		scan(root + L"\\Scripts", "Scripts/");
		CreateDirectoryW((root + L"\\Scripts").c_str(), nullptr);

		if (ImGui::BeginChild("###gw2igh_tt_luas", ImVec2(0.f, 70.f), true))
		{
			if (luas.empty())
				ImGui::TextDisabled("No .lua files yet - Open folder and add Scripts/foo.lua");
			for (const std::string& f : luas)
				ImGui::BulletText("%s", f.c_str());
		}
		ImGui::EndChild();
	}
}
