#include "PathingIndex.h"

#include "Globals.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>
#include <shlobj.h>

namespace PathingDetail
{
void DiscoverPackDirs(std::vector<std::wstring>& dirs)
{
	auto canonicalize = [](const std::wstring& d) -> std::wstring
	{
		wchar_t full[MAX_PATH]{};
		const DWORD n = GetFullPathNameW(d.c_str(), MAX_PATH, full, nullptr);
		if (n > 0 && n < MAX_PATH)
			return full;
		return d;
	};

	auto add = [&](const std::wstring& d)
	{
		const std::wstring canon = canonicalize(d);
		DWORD attr = GetFileAttributesW(canon.c_str());
		if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
			return;
		for (const std::wstring& e : dirs)
			if (_wcsicmp(e.c_str(), canon.c_str()) == 0)
				return;
		dirs.push_back(canon);
	};

	/* Our bundled pack lives here - no other addons required. */
	auto addOurs = [&](const std::wstring& addons)
	{
		add(addons + L"\\GW2-InGame-Helper\\pathing");
		add(addons + L"\\GW2-InGame-Helper\\pathing"); /* shipping pack if present */
		/* Reuse pack if already installed for Minimap Resizer. */
		add(addons + L"\\GW2-MinimapResizer\\pathing");
	};

	/* Optional fallbacks only if the user already has packs elsewhere. */
	auto addFallbacks = [&](const std::wstring& addons)
	{
		add(addons + L"\\Taimi\\pathing");
		add(addons + L"\\blishhud\\markers");
		add(addons + L"\\GW2TacO\\POIs");
	};

	auto addFromGameRoot = [&](const std::wstring& root)
	{
		addOurs(root + L"\\addons");
		addFallbacks(root + L"\\addons");
	};

	/* Prefer our DLL path (.../addons/GW2-InGame-Helper[/].dll) - reliable under Wine. */
	if (G::Self)
	{
		wchar_t img[MAX_PATH]{};
		const DWORD n = GetModuleFileNameW(G::Self, img, MAX_PATH);
		if (n > 0 && n < MAX_PATH)
		{
			std::wstring p(img);
			size_t slash = p.find_last_of(L"\\/");
			if (slash != std::wstring::npos)
				p = p.substr(0, slash); /* directory containing the DLL */
			slash = p.find_last_of(L"\\/");
			if (slash != std::wstring::npos)
			{
				const std::wstring leaf = p.substr(slash + 1);
				if (_wcsicmp(leaf.c_str(), L"GW2-InGame-Helper") == 0 ||
					_wcsicmp(leaf.c_str(), L"GW2-InGame-Helper") == 0)
				{
					add(p + L"\\pathing");
					addOurs(p.substr(0, slash));
					addFallbacks(p.substr(0, slash));
				}
				else
				{
					addOurs(p); /* DLL lived directly in addons/ */
					addFallbacks(p);
				}
			}
		}
	}

	if (G::API && G::API->Paths_GetAddonDirectory)
	{
		const char* ad = G::API->Paths_GetAddonDirectory(ADDON_NAME);
		if (ad && ad[0])
		{
			wchar_t wad[MAX_PATH]{};
			if (MultiByteToWideChar(CP_UTF8, 0, ad, -1, wad, MAX_PATH) > 0)
			{
				std::wstring p(wad);
				while (!p.empty() && (p.back() == L'\\' || p.back() == L'/'))
					p.pop_back();
				add(p + L"\\pathing");
				const size_t slash = p.find_last_of(L"\\/");
				if (slash != std::wstring::npos)
				{
					addOurs(p.substr(0, slash));
					addFallbacks(p.substr(0, slash));
				}
			}
		}
	}

	/* Gw2-64.exe directory -> game root\addons\... */
	wchar_t exe[MAX_PATH]{};
	const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
	if (n > 0 && n < MAX_PATH)
	{
		std::wstring p(exe);
		const size_t slash = p.find_last_of(L"\\/");
		if (slash != std::wstring::npos)
			addFromGameRoot(p.substr(0, slash));
	}

	wchar_t docs[MAX_PATH]{};
	if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, docs)))
	{
		std::wstring d(docs);
		add(d + L"\\Guild Wars 2\\addons\\blishhud\\markers");
		add(d + L"\\Guild Wars 2\\addons\\GW2TacO\\POIs");
	}
}

bool IsOurPathingDir(const std::wstring& dir)
{
	std::wstring low;
	low.reserve(dir.size());
	for (wchar_t c : dir)
	{
		if (c >= L'A' && c <= L'Z')
			low.push_back(static_cast<wchar_t>(c - L'A' + L'a'));
		else if (c == L'/')
			low.push_back(L'\\');
		else
			low.push_back(c);
	}
	return low.find(L"gw2-ingame-helper\\pathing") != std::wstring::npos;
}

std::wstring LeafLower(const std::wstring& path)
{
	size_t slash = path.find_last_of(L"\\/");
	std::wstring leaf = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
	for (wchar_t& c : leaf)
	{
		if (c >= L'A' && c <= L'Z')
			c = static_cast<wchar_t>(c - L'A' + L'a');
		else if (c == L'/')
			c = L'\\';
	}
	return leaf;
}

std::string WideLeafUtf8(const std::wstring& path)
{
	size_t slash = path.find_last_of(L"\\/");
	const std::wstring leaf = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
	char buf[MAX_PATH]{};
	if (WideCharToMultiByte(CP_UTF8, 0, leaf.c_str(), -1, buf, MAX_PATH, nullptr, nullptr) > 0)
		return buf;
	std::string fallback;
	for (wchar_t c : leaf)
		if (c < 128)
			fallback.push_back(static_cast<char>(c));
	return fallback;
}

void ListTacoFiles(const std::wstring& dir, std::vector<std::wstring>& out, bool tekkitOnly)
{
	const std::wstring pattern = dir + L"\\*.taco";
	WIN32_FIND_DATAW fd{};
	HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE)
		return;
	do
	{
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		std::wstring name = fd.cFileName;
		/* Reject stamp files like Foo.taco.ver (Wine *.taco can match them). */
		{
			std::wstring low = name;
			for (wchar_t& c : low)
				if (c >= L'A' && c <= L'Z')
					c = static_cast<wchar_t>(c - L'A' + L'a');
			if (low.size() < 5 || low.compare(low.size() - 5, 5, L".taco") != 0)
				continue;
		}
		if (tekkitOnly)
		{
			std::wstring low = name;
			for (wchar_t& c : low)
				if (c >= L'A' && c <= L'Z')
					c = static_cast<wchar_t>(c - L'A' + L'a');
			if (low.find(L"tekkit") == std::wstring::npos)
				continue;
		}
		wchar_t fullBuf[MAX_PATH]{};
		const std::wstring joined = dir + L"\\" + fd.cFileName;
		const DWORD n = GetFullPathNameW(joined.c_str(), MAX_PATH, fullBuf, nullptr);
		const std::wstring full = (n > 0 && n < MAX_PATH) ? fullBuf : joined;
		const std::wstring leafKey = LeafLower(full);

		bool dup = false;
		for (const std::wstring& e : out)
		{
			if (_wcsicmp(e.c_str(), full.c_str()) == 0 || LeafLower(e) == leafKey)
			{
				dup = true;
				break;
			}
		}
		if (!dup)
			out.push_back(full);
	} while (FindNextFileW(h, &fd));
	FindClose(h);
}

/* Drop alias copies of curated packs (same content, different filename).
   e.g. "Tekkit's All-In-One.taco" next to curated tw_ALL_IN_ONE.taco -> every
   route draws twice in-world. */
void SuppressDuplicateTacoPacks(std::vector<std::wstring>& packs)
{
	bool hasCuratedTekkit = false;
	for (const std::wstring& p : packs)
	{
		if (LeafLower(p) == L"tw_all_in_one.taco")
		{
			hasCuratedTekkit = true;
			break;
		}
	}

	auto isTekkitAioAlias = [](const std::wstring& leaf) -> bool
	{
		if (leaf == L"tw_all_in_one.taco")
			return false;
		const bool hasTekkit = leaf.find(L"tekkit") != std::wstring::npos;
		const bool hasAio =
			leaf.find(L"all_in_one") != std::wstring::npos ||
			leaf.find(L"all-in-one") != std::wstring::npos ||
			leaf.find(L"allinone") != std::wstring::npos;
		return hasTekkit && hasAio;
	};

	if (hasCuratedTekkit)
	{
		packs.erase(
			std::remove_if(packs.begin(), packs.end(),
				[&](const std::wstring& p) { return isTekkitAioAlias(LeafLower(p)); }),
			packs.end());
	}

	/* Exact byte-size clones of an already-kept pack (any author). */
	std::vector<std::pair<std::wstring, ULONGLONG>> kept;
	kept.reserve(packs.size());
	std::vector<std::wstring> filtered;
	filtered.reserve(packs.size());
	for (const std::wstring& p : packs)
	{
		WIN32_FILE_ATTRIBUTE_DATA fad{};
		ULONGLONG sz = 0;
		if (GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fad))
			sz = (static_cast<ULONGLONG>(fad.nFileSizeHigh) << 32) | fad.nFileSizeLow;
		bool clone = false;
		if (sz > 0)
		{
			for (const auto& k : kept)
			{
				if (k.second == sz)
				{
					clone = true;
					break;
				}
			}
		}
		if (clone)
			continue;
		kept.push_back({p, sz});
		filtered.push_back(p);
	}
	packs.swap(filtered);
}

} // namespace PathingDetail
