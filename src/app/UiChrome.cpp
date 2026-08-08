#include "UiChrome.h"

#include "AddonPaths.h"
#include "Globals.h"

#include "miniz.h"
#include "nexus/Nexus.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

/* ld -r -b binary build/ui_chrome.zip */
extern "C" const unsigned char _binary_build_ui_chrome_zip_start[];
extern "C" const unsigned char _binary_build_ui_chrome_zip_end[];
/* GNU ld: value of this symbol IS the byte size (not a pointer to size). */
extern "C" const unsigned char _binary_build_ui_chrome_zip_size[];

namespace
{
	constexpr const char* kPackStamp = "uc36";
	constexpr int kChromeIds[] = {
		155985, 155981, 156022, 156008, 156009, 156010, 155967, 156260, 155014,
		/* Curated rail / Log Manager icons (Desktop/icons — current set). */
		156081, 240678, 563466, 563468, 699005, 834008,
		866115, 866117, 866119, 866124, 1948130,
		1228263, 1228855, 2199974, 2596974, 2596976,
		3124871, 3443174, 3443175, 3713037,
		60970, 155867, /* Pathing diamond + Trail Tools map */
		561441, /* Vault gold star */
		/* Scrollbar DAT ids (also as named files below). */
		154969, 154970, 154971, 154973, 155031, 156078, 154968,
		/* Plaque / button DAT mirrors (named copies preferred). */
		155146, 1692715, 155084, 155076, 156013, 1701860, 1670507
	};
	/* Named pack files (not numeric DAT ids). */
	constexpr const char* kChromeNamed[] = {
		"button-exit.png", "button-exit-active.png", "crest-hero.png",
		"browse-hero.png",
		"panel-wash.png", "title-bar.png", "panel-edge.png", "ink-edge.png",
		"scroll-thumb.png", "scroll-thumb-mid.png", "scroll-thumb-top.png",
		"scroll-thumb-cap.png", "scroll-arrow.png", "scroll-arrow-up.png",
		"scroll-atlas.png", "scroll-track-h.png",
		/* HTML + pad plaque set */
		"card-fill.png", "card-fill-dark.png", "card-border.png",
		"hero-plate.png", "btn-frame.png", "btn-frame-hover.png", "btn-plate.png",
		"divider-gold.png", "header-ornament.png", "plaque-corner.png"
	};

	std::string WideToUtf8(const std::wstring& w)
	{
		if (w.empty())
			return {};
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string out(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
		if (n > 0)
			WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), n, nullptr, nullptr);
		return out;
	}

	std::string PathToFileUrl(const std::wstring& path)
	{
		std::string utf8 = WideToUtf8(path);
		for (char& c : utf8)
		{
			if (c == '\\')
				c = '/';
		}
		if (utf8.size() >= 2 && utf8[1] == ':')
			return std::string("file:///") + utf8;
		return std::string("file://") + utf8;
	}

	std::wstring ChromeDir(const std::wstring& addonDir)
	{
		return addonDir + L"\\ui-chrome";
	}

	bool WriteBytes(const std::wstring& path, const void* data, DWORD len)
	{
		HANDLE out = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (out == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(out, data, len, &written, nullptr);
		CloseHandle(out);
		return ok && written == len;
	}

	bool StampMatches(const std::wstring& verPath)
	{
		HANDLE vin = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (vin == INVALID_HANDLE_VALUE)
			return false;
		char buf[32]{};
		DWORD got = 0;
		bool ok = false;
		if (ReadFile(vin, buf, sizeof(buf) - 1, &got, nullptr) && got > 0)
			ok = (std::strncmp(buf, kPackStamp, std::strlen(kPackStamp)) == 0);
		CloseHandle(vin);
		return ok;
	}

	bool FileExists(const std::wstring& path)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		CloseHandle(h);
		return true;
	}

	bool ExtractPack(const std::wstring& addonDir)
	{
		/* Prefer size symbol (GNU ld) — more reliable than end-start across PE refptrs. */
		const unsigned char* begin = _binary_build_ui_chrome_zip_start;
		size_t size = static_cast<size_t>(
			_binary_build_ui_chrome_zip_end - _binary_build_ui_chrome_zip_start);
		const size_t sizeSym = reinterpret_cast<size_t>(&_binary_build_ui_chrome_zip_size);
		if (sizeSym > 64 && sizeSym < (64ull * 1024ull * 1024ull))
			size = sizeSym;
		if (!begin || size < 64)
		{
			CreateDirectoryW(addonDir.c_str(), nullptr);
			CreateDirectoryW(ChromeDir(addonDir).c_str(), nullptr);
			WriteBytes(ChromeDir(addonDir) + L"\\extract.err", "bad blob", 8);
			return false;
		}

		const std::wstring dir = ChromeDir(addonDir);
		const std::wstring verPath = dir + L"\\ui-chrome.ver";
		const std::wstring probe = dir + L"\\button-exit.png";

		if (StampMatches(verPath) && FileExists(probe))
			return true;

		CreateDirectoryW(addonDir.c_str(), nullptr);
		CreateDirectoryW(dir.c_str(), nullptr);

		mz_zip_archive zip{};
		if (!mz_zip_reader_init_mem(&zip, begin, size, 0))
		{
			WriteBytes(dir + L"\\extract.err", "zip init", 8);
			return false;
		}

		bool ok = true;
		const mz_uint n = mz_zip_reader_get_num_files(&zip);
		for (mz_uint i = 0; i < n; ++i)
		{
			mz_zip_archive_file_stat st{};
			if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory)
				continue;
			std::string name = st.m_filename;
			if (name.empty() || name.find("..") != std::string::npos ||
				name.find('/') != std::string::npos || name.find('\\') != std::string::npos)
			{
				ok = false;
				break;
			}
			size_t outLen = 0;
			void* mem = mz_zip_reader_extract_to_heap(&zip, i, &outLen, 0);
			if (!mem)
			{
				ok = false;
				break;
			}
			const std::wstring outPath = dir + L"\\" + std::wstring(name.begin(), name.end());
			const bool wrote = WriteBytes(outPath, mem, static_cast<DWORD>(outLen));
			mz_free(mem);
			if (!wrote)
			{
				ok = false;
				break;
			}
		}
		mz_zip_reader_end(&zip);
		if (!ok)
		{
			WriteBytes(dir + L"\\extract.err", "extract", 7);
			return false;
		}

		return WriteBytes(verPath, kPackStamp, static_cast<DWORD>(std::strlen(kPackStamp)));
	}
}

void UiChrome::MakeTexId(int assetId, char* out, size_t outLen)
{
	/* Stamp in the id so Nexus drops stale GPU caches when the pack bumps. */
	std::snprintf(out, outLen, "GW2IGH_CHROME_%s_%d", kPackStamp, assetId);
}

void UiChrome::MakeNamedTexId(const char* fileStem, char* out, size_t outLen)
{
	std::snprintf(out, outLen, "GW2IGH_CHROME_%s_%s", kPackStamp,
		fileStem ? fileStem : "x");
}

bool UiChrome::Ensure(const std::wstring& addonDir)
{
	if (addonDir.empty())
		return false;
	return ExtractPack(addonDir);
}

std::wstring UiChrome::PngPath(const std::wstring& addonDir, int assetId)
{
	if (addonDir.empty() || assetId <= 0)
		return {};
	char name[32];
	std::snprintf(name, sizeof(name), "%d.png", assetId);
	return NamedPngPath(addonDir, name);
}

std::wstring UiChrome::NamedPngPath(const std::wstring& addonDir, const char* fileName)
{
	if (addonDir.empty() || !fileName || !fileName[0])
		return {};
	std::wstring path = ChromeDir(addonDir) + L"\\";
	for (const char* p = fileName; *p; ++p)
		path.push_back(static_cast<wchar_t>(*p));
	if (!FileExists(path))
		return {};
	return path;
}

std::string UiChrome::FillFileUrl(const std::wstring& addonDir, int assetId)
{
	Ensure(addonDir);
	/* Prefer opaque rectangular wash — full 155985 has feathered alpha edges that
	   leave black gaps at the sides/bottom of CEF pages under background-size:cover. */
	const std::wstring wash = NamedPngPath(addonDir, "panel-wash.png");
	if (!wash.empty())
		return PathToFileUrl(wash);
	if (assetId <= 0)
		assetId = 155985;
	const std::wstring path = PngPath(addonDir, assetId);
	if (path.empty())
		return {};
	return PathToFileUrl(path);
}

std::string UiChrome::NamedFileUrl(const std::wstring& addonDir, const char* fileName)
{
	Ensure(addonDir);
	const std::wstring path = NamedPngPath(addonDir, fileName);
	if (path.empty())
		return {};
	return PathToFileUrl(path);
}

std::string UiChrome::DecorCss(const std::wstring& addonDir)
{
	Ensure(addonDir);
	/* Only textures still referenced by rules below — skip card-fill / btn-frame
	   (asymmetric strips + Wine CEF file:// thrash). */
	const std::string hero = NamedFileUrl(addonDir, "hero-plate.png");
	const std::string div = NamedFileUrl(addonDir, "divider-gold.png");
	const std::string corner = NamedFileUrl(addonDir, "plaque-corner.png");
	const std::string orn = NamedFileUrl(addonDir, "header-ornament.png");
	if (hero.empty() && div.empty() && corner.empty() && orn.empty())
		return {};

	std::string s;
	s.reserve(2800);
	s += "\n/* ui-chrome decor */\n:root {\n";
	if (!hero.empty()) { s += "  --chrome-hero: url(\""; s += hero; s += "\");\n"; }
	if (!div.empty()) { s += "  --chrome-divider: url(\""; s += div; s += "\");\n"; }
	if (!corner.empty()) { s += "  --chrome-corner: url(\""; s += corner; s += "\");\n"; }
	if (!orn.empty()) { s += "  --chrome-ornament: url(\""; s += orn; s += "\");\n"; }
	s += "}\n";

	s += R"CSS(
/* Plain plaque panels — no asymmetric card-fill strips (those top-align badly). */
.hero, .plaque, section.block, a.tile, .modal {
  background-image:
    linear-gradient(165deg, rgba(48, 38, 22, 0.42) 0%, transparent 48%),
    linear-gradient(180deg, rgba(20, 16, 12, 0.92), rgba(10, 8, 6, 0.96));
  background-size: auto, auto;
  background-position: center, center;
  background-repeat: no-repeat;
  border: 1px solid var(--border);
  box-shadow:
    inset 0 1px 0 rgba(255, 230, 160, 0.14),
    inset 0 0 0 1px rgba(0, 0, 0, 0.35),
    0 10px 32px rgba(0, 0, 0, 0.48);
  position: relative;
}
.hero {
  background-image:
    linear-gradient(105deg, rgba(14, 11, 8, 0.55) 0%, rgba(14, 11, 8, 0.12) 55%, transparent 78%),
    linear-gradient(180deg, rgba(30, 24, 16, 0.55), rgba(10, 8, 6, 0.88)),
    var(--chrome-hero, none);
  background-size: auto, auto, cover;
  background-position: left center, center, center;
  background-repeat: no-repeat;
}
.plaque::after, section.block::after {
  content: none;
}
.hero::after { content: none !important; background: none !important; }
.plaque > *, section.block > *, .hero > *, a.tile > * { position: relative; z-index: 1; }
section.block > .head {
  background-image:
    linear-gradient(90deg, rgba(61, 48, 24, 0.92) 0%, rgba(26, 21, 16, 0.85) 70%),
    var(--chrome-ornament, none);
  background-size: auto, 100% 100%;
  background-position: left center, center;
  background-repeat: no-repeat;
}
/* TOC / chips / tiles: plain fill + gold rim (no btn-frame textures). */
nav.toc a, a.chip, a.jump, a.tile, .cta, button.gw2-btn {
  color: var(--gold-dim);
  text-decoration: none;
  border: 1px solid var(--border-deep);
  background-color: var(--accent);
  background-image: none;
  box-shadow: none;
}
a.tile {
  background-image:
    linear-gradient(165deg, rgba(48, 38, 22, 0.35) 0%, transparent 48%),
    linear-gradient(180deg, rgba(20, 16, 12, 0.92), rgba(10, 8, 6, 0.96));
}
nav.toc a:hover, a.chip:hover, a.jump:hover, a.tile:hover, .cta:hover, button.gw2-btn:hover,
nav.toc a:focus-visible, a.chip:focus-visible, a.jump:focus-visible, a.tile:focus-visible,
.cta:focus-visible, button.gw2-btn:focus-visible {
  color: var(--gold-bright);
  border-color: var(--gold);
  background-color: rgba(40, 32, 20, 0.55);
  box-shadow: inset 0 0 0 1px rgba(232, 196, 112, 0.22);
}
a.tile:hover {
  background-image:
    linear-gradient(165deg, rgba(60, 48, 28, 0.45) 0%, transparent 48%),
    linear-gradient(180deg, rgba(28, 22, 14, 0.95), rgba(12, 10, 8, 0.98));
}
.hairline, hr.chrome-div {
  height: 10px;
  border: 0;
  margin: 0.85rem 0;
  background-color: transparent;
  background-image: var(--chrome-divider, linear-gradient(90deg, transparent, var(--border), transparent));
  background-repeat: no-repeat;
  background-position: center;
  background-size: 100% 100%;
}
.plaque-corner-tl, .plaque-corner-tr, .plaque-corner-bl, .plaque-corner-br {
  pointer-events: none;
  position: absolute;
  width: 36px;
  height: 36px;
  z-index: 2;
  background-image: var(--chrome-corner, none);
  background-size: contain;
  background-repeat: no-repeat;
  opacity: 0.85;
}
.plaque-corner-tl { top: -2px; left: -2px; }
.plaque-corner-tr { top: -2px; right: -2px; transform: scaleX(-1); }
.plaque-corner-bl { bottom: -2px; left: -2px; transform: scaleY(-1); }
.plaque-corner-br { bottom: -2px; right: -2px; transform: scale(-1); }
)CSS";
	return s;
}

void UiChrome::WarmTextures(const std::wstring& addonDir)
{
	/* Always extract first — do not gate on Texture APIs (they may be late-bound). */
	if (!Ensure(addonDir))
	{
		if (G::API && G::API->Log)
			G::API->Log(LOGL_WARNING, ADDON_NAME, "ui-chrome pack extract failed");
		return;
	}
	if (!G::API || !G::API->Textures_GetOrCreateFromFile)
		return;
	for (int id : kChromeIds)
	{
		const std::wstring path = PngPath(addonDir, id);
		if (path.empty())
			continue;
		char texId[48];
		MakeTexId(id, texId, sizeof(texId));
		const std::string utf8 = WideToUtf8(path);
		G::API->Textures_GetOrCreateFromFile(texId, utf8.c_str());
	}
	for (const char* name : kChromeNamed)
	{
		const std::wstring path = NamedPngPath(addonDir, name);
		if (path.empty())
			continue;
		/* Stem without .png for stable tex id. */
		char stem[64];
		std::snprintf(stem, sizeof(stem), "%s", name);
		if (char* dot = std::strrchr(stem, '.'))
			*dot = '\0';
		char texId[80];
		MakeNamedTexId(stem, texId, sizeof(texId));
		const std::string utf8 = WideToUtf8(path);
		G::API->Textures_GetOrCreateFromFile(texId, utf8.c_str());
	}
}
