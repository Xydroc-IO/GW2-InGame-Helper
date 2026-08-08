#include "UserTheme.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "HelperTheme.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace
{
	std::string gCssOverride;
	bool gCustom = false;

	std::string WideToUtf8(const std::wstring& w)
	{
		if (w.empty())
			return {};
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (n <= 0)
			return {};
		std::string out(static_cast<size_t>(n - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
		return out;
	}

	std::wstring Utf8ToWide(const char* utf8)
	{
		if (!utf8 || !utf8[0])
			return {};
		int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
		if (n <= 0)
			return {};
		std::wstring out(static_cast<size_t>(n - 1), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), n);
		return out;
	}

	bool WriteUtf8File(const std::wstring& path, const char* data, size_t len)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, data, static_cast<DWORD>(len), &written, nullptr);
		CloseHandle(h);
		return ok && written == len;
	}

	bool ReadUtf8File(const std::wstring& path, std::string& out)
	{
		out.clear();
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		LARGE_INTEGER sz{};
		if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 0 || sz.QuadPart > 256 * 1024)
		{
			CloseHandle(h);
			return false;
		}
		if (sz.QuadPart == 0)
		{
			CloseHandle(h);
			return true;
		}
		out.resize(static_cast<size_t>(sz.QuadPart));
		DWORD got = 0;
		const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &got, nullptr);
		CloseHandle(h);
		if (!ok) { out.clear(); return false; }
		out.resize(got);
		return true;
	}

	int HexNibble(char c)
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	}

	bool ParseHexColor(const char* s, ImVec4& out)
	{
		if (!s || s[0] != '#')
			return false;
		++s;
		int digits[8]{};
		int n = 0;
		while (*s && n < 8)
		{
			const int d = HexNibble(*s++);
			if (d < 0)
				break;
			digits[n++] = d;
		}
		auto pack = [&](int i) -> float {
			return static_cast<float>((digits[i] << 4) | digits[i + 1]) / 255.f;
		};
		if (n == 6)
		{
			out = ImVec4(pack(0), pack(2), pack(4), 1.f);
			return true;
		}
		if (n == 8)
		{
			out = ImVec4(pack(0), pack(2), pack(4), pack(6));
			return true;
		}
		if (n == 3)
		{
			out = ImVec4(
				static_cast<float>((digits[0] << 4) | digits[0]) / 255.f,
				static_cast<float>((digits[1] << 4) | digits[1]) / 255.f,
				static_cast<float>((digits[2] << 4) | digits[2]) / 255.f,
				1.f);
			return true;
		}
		return false;
	}

	void AppendCssHex(std::string& css, const char* var, const ImVec4& c)
	{
		char buf[48];
		const int r = static_cast<int>(c.x * 255.f + 0.5f);
		const int g = static_cast<int>(c.y * 255.f + 0.5f);
		const int b = static_cast<int>(c.z * 255.f + 0.5f);
		if (c.w < 0.999f)
		{
			const int a = static_cast<int>(c.w * 255.f + 0.5f);
			std::snprintf(buf, sizeof(buf), "    %s: #%02x%02x%02x%02x;\n", var, r, g, b, a);
		}
		else
			std::snprintf(buf, sizeof(buf), "    %s: #%02x%02x%02x;\n", var, r, g, b);
		css += buf;
	}

	void BuildCssFromTokens()
	{
		gCssOverride.clear();
		if (!gCustom)
			return;
		gCssOverride = "\n  /* user-theme */\n  :root {\n";
		AppendCssHex(gCssOverride, "--gold", HelperTheme::Gold);
		AppendCssHex(gCssOverride, "--gold-bright", HelperTheme::GoldBright);
		AppendCssHex(gCssOverride, "--gold-dim", HelperTheme::GoldDim);
		AppendCssHex(gCssOverride, "--gold-muted", HelperTheme::GoldMuted);
		AppendCssHex(gCssOverride, "--text", HelperTheme::Ink);
		AppendCssHex(gCssOverride, "--muted", HelperTheme::Muted);
		AppendCssHex(gCssOverride, "--bg", HelperTheme::Bg);
		AppendCssHex(gCssOverride, "--panel", HelperTheme::Panel);
		AppendCssHex(gCssOverride, "--panel-solid", HelperTheme::Panel);
		AppendCssHex(gCssOverride, "--border", HelperTheme::Border);
		AppendCssHex(gCssOverride, "--ok", HelperTheme::Ok);
		AppendCssHex(gCssOverride, "--warn", HelperTheme::Warn);
		AppendCssHex(gCssOverride, "--header", HelperTheme::Header);
		AppendCssHex(gCssOverride, "--accent", HelperTheme::TabIdle);
		gCssOverride += "  }\n";
	}

	bool ApplyKey(const char* key, const ImVec4& c)
	{
		if (std::strcmp(key, "gold") == 0) { HelperTheme::Gold = c; return true; }
		if (std::strcmp(key, "gold_bright") == 0) { HelperTheme::GoldBright = c; return true; }
		if (std::strcmp(key, "gold_dim") == 0) { HelperTheme::GoldDim = c; return true; }
		if (std::strcmp(key, "gold_muted") == 0) { HelperTheme::GoldMuted = c; return true; }
		if (std::strcmp(key, "ink") == 0 || std::strcmp(key, "text") == 0)
		{ HelperTheme::Ink = c; return true; }
		if (std::strcmp(key, "muted") == 0) { HelperTheme::Muted = c; return true; }
		if (std::strcmp(key, "bg") == 0) { HelperTheme::Bg = c; return true; }
		if (std::strcmp(key, "panel") == 0) { HelperTheme::Panel = c; return true; }
		if (std::strcmp(key, "child") == 0) { HelperTheme::Child = c; return true; }
		if (std::strcmp(key, "border") == 0) { HelperTheme::Border = c; return true; }
		if (std::strcmp(key, "tab_active") == 0) { HelperTheme::TabActive = c; return true; }
		if (std::strcmp(key, "tab_idle") == 0) { HelperTheme::TabIdle = c; return true; }
		if (std::strcmp(key, "header") == 0) { HelperTheme::Header = c; return true; }
		if (std::strcmp(key, "ok") == 0) { HelperTheme::Ok = c; return true; }
		if (std::strcmp(key, "warn") == 0) { HelperTheme::Warn = c; return true; }
		return false;
	}

	bool ParseThemeIni(const std::string& body)
	{
		bool any = false;
		size_t i = 0;
		while (i < body.size())
		{
			size_t e = body.find('\n', i);
			if (e == std::string::npos) e = body.size();
			std::string line = body.substr(i, e - i);
			i = e + 1;
			while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
				line.pop_back();
			size_t start = 0;
			while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
				++start;
			if (start >= line.size() || line[start] == '#' || line[start] == ';')
				continue;
			const size_t eq = line.find('=', start);
			if (eq == std::string::npos)
				continue;
			std::string key = line.substr(start, eq - start);
			while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
				key.pop_back();
			for (char& c : key)
				c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
			if (key == "name")
				continue;
			size_t vs = eq + 1;
			while (vs < line.size() && (line[vs] == ' ' || line[vs] == '\t'))
				++vs;
			std::string val = line.substr(vs);
			while (!val.empty() && (val.back() == ' ' || val.back() == '\t'))
				val.pop_back();
			ImVec4 col{};
			if (!ParseHexColor(val.c_str(), col))
				continue;
			if (ApplyKey(key.c_str(), col))
				any = true;
		}
		return any;
	}

	constexpr const char* kReadme =
		"GW2 In-Game Helper — user themes\n"
		"================================\n"
		"\n"
		"Put a folder here with a theme.ini inside:\n"
		"\n"
		"  config/themes/my-theme/theme.ini\n"
		"\n"
		"Then pick it in Settings → Theme.\n"
		"\n"
		"theme.ini keys (hex #RRGGBB or #RRGGBBAA; omit to keep default):\n"
		"  gold, gold_bright, gold_dim, gold_muted\n"
		"  text (or ink), muted, bg, panel, child, border\n"
		"  tab_active, tab_idle, header, ok, warn\n"
		"  name=Display label (optional, ignored by loader)\n"
		"\n"
		"Applies to ImGui pads and built-in helper pages (Home, Live Panels,\n"
		"Cheat Sheets, etc.). External browse sites and ui-chrome PNGs are not themed.\n"
		"After changing a theme.ini, use Reload themes in Settings (or reopen the page).\n";

	constexpr const char* kExampleIni =
		"name=High contrast\n"
		"gold=#ffe566\n"
		"gold_bright=#fff0a0\n"
		"text=#ffffff\n"
		"muted=#c8c8c8\n"
		"bg=#000000\n"
		"panel=#1a1a1a\n"
		"border=#ffcc00\n"
		"ok=#66ff66\n"
		"warn=#ffaa44\n"
		"tab_active=#333300\n"
		"tab_idle=#111111\n"
		"header=#2a2a00\n";

	bool ThemeIdSafe(const char* id)
	{
		if (!id || !id[0])
			return false;
		/* Folder name only — reject path traversal / separators. */
		size_t n = 0;
		for (const char* p = id; *p; ++p, ++n)
		{
			if (n >= 63)
				return false;
			const unsigned char c = static_cast<unsigned char>(*p);
			if (c < 32 || c == '/' || c == '\\' || c == ':' || c == '*' ||
				c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || c == '.')
				return false;
		}
		return n > 0;
	}
}

void UserTheme::EnsureSeed()
{
	const std::wstring dir = AddonPaths::ThemesDir();
	if (dir.empty())
		return;
	const std::wstring readme = dir + L"\\README.txt";
	if (GetFileAttributesW(readme.c_str()) == INVALID_FILE_ATTRIBUTES)
		WriteUtf8File(readme, kReadme, std::strlen(kReadme));

	const std::wstring exDir = dir + L"\\example-high-contrast";
	CreateDirectoryW(exDir.c_str(), nullptr);
	const std::wstring exIni = exDir + L"\\theme.ini";
	if (GetFileAttributesW(exIni.c_str()) == INVALID_FILE_ATTRIBUTES)
		WriteUtf8File(exIni, kExampleIni, std::strlen(kExampleIni));
}

std::vector<std::string> UserTheme::ListThemes()
{
	std::vector<std::string> out;
	const std::wstring dir = AddonPaths::ThemesDir();
	if (dir.empty())
		return out;
	const std::wstring pattern = dir + L"\\*";
	WIN32_FIND_DATAW fd{};
	HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE)
		return out;
	do
	{
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			continue;
		if (fd.cFileName[0] == L'.' &&
			(fd.cFileName[1] == 0 || (fd.cFileName[1] == L'.' && fd.cFileName[2] == 0)))
			continue;
		const std::wstring ini = dir + L"\\" + fd.cFileName + L"\\theme.ini";
		if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES)
			continue;
		std::string name = WideToUtf8(fd.cFileName);
		if (!ThemeIdSafe(name.c_str()))
			continue;
		out.push_back(std::move(name));
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	std::sort(out.begin(), out.end());
	return out;
}

bool UserTheme::Apply(const char* id)
{
	HelperTheme::ResetToBuiltin();
	gCssOverride.clear();
	gCustom = false;

	if (!id || !id[0] || std::strcmp(id, "default") == 0)
		return true;
	if (!ThemeIdSafe(id))
		return false;

	const std::wstring dir = AddonPaths::ThemesDir();
	if (dir.empty())
		return false;
	const std::wstring ini = dir + L"\\" + Utf8ToWide(id) + L"\\theme.ini";
	std::string body;
	if (!ReadUtf8File(ini, body))
		return false;

	HelperTheme::ResetToBuiltin();
	if (!ParseThemeIni(body))
	{
		/* Empty/invalid file — stay on builtins but still mark selected. */
		gCustom = false;
		BuildCssFromTokens();
		return true;
	}
	gCustom = true;
	BuildCssFromTokens();
	return true;
}

void UserTheme::Reload()
{
	EnsureSeed();
	if (!G::ThemeId[0] || std::strcmp(G::ThemeId, "default") == 0)
		Apply("default");
	else if (!Apply(G::ThemeId))
	{
		/* Missing theme folder — fall back to default without clearing preference. */
		HelperTheme::ResetToBuiltin();
		gCustom = false;
		gCssOverride.clear();
	}
}

const std::string& UserTheme::CssRootOverride()
{
	return gCssOverride;
}

bool UserTheme::IsCustomActive()
{
	return gCustom;
}
