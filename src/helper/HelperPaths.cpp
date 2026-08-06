/* Helper path helpers (pages/cmds under helper dir) — HelperDetail. */
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "HelperInternal.h"

namespace HelperDetail
{
	std::wstring HelperDir()
	{
		wchar_t path[MAX_PATH]{};
		if (!GetModuleFileNameW(nullptr, path, MAX_PATH))
			return {};
		std::wstring full = path;
		const size_t slash = full.find_last_of(L"\\/");
		if (slash == std::wstring::npos)
			return {};
		return full.substr(0, slash);
	}

	/* Mirror AddonPaths::EnsureUnder — helper exe cannot link Nexus AddonPaths. */
	std::wstring EnsureHelperUnder(const std::wstring& root, const wchar_t* relative)
	{
		if (root.empty() || !relative || !relative[0])
			return {};
		std::wstring cur = root;
		const wchar_t* p = relative;
		while (*p)
		{
			while (*p == L'\\' || *p == L'/')
				++p;
			if (!*p)
				break;
			const wchar_t* start = p;
			while (*p && *p != L'\\' && *p != L'/')
				++p;
			cur.push_back(L'\\');
			cur.append(start, p);
			CreateDirectoryW(cur.c_str(), nullptr);
		}
		return cur;
	}

	std::wstring HelperPagesDir()
	{
		return EnsureHelperUnder(HelperDir(), L"pages");
	}

	std::wstring HelperCmdsDir()
	{
		return EnsureHelperUnder(HelperDir(), L"cmds");
	}

	std::string WidePathToFileUrl(const std::wstring& path)
	{
		std::string utf8 = WideToUtf8(path);
		for (char& c : utf8)
		{
			if (c == '\\')
				c = '/';
		}
		if (utf8.size() >= 2 && utf8[1] == ':')
			return std::string("file:///") + utf8;
		if (!utf8.empty() && utf8[0] == '/')
			return std::string("file://") + utf8;
		return std::string("file:///") + utf8;
	}


} // namespace HelperDetail
