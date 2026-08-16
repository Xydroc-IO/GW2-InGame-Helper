#include "CheatSheets.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "HelperThemeCss.h"
#include "UiChrome.h"
#include "UserTheme.h"

#include "miniz.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

/* ld -r -b binary build/embed/cheatsheets.zip */
extern "C" const unsigned char _binary_cheatsheets_zip_start[];
extern "C" const unsigned char _binary_cheatsheets_zip_end[];

namespace
{
	constexpr const char* kPackStamp = "c2228";

	struct OwnedSheet
	{
		std::string id;
		std::string about;
		std::string fileStem;
		std::string version;
		std::string browseLabel;
		std::string browseTitle;
		CheatSheets::Sheet view{};
	};

	std::vector<OwnedSheet> gOwned;
	std::vector<CheatSheets::Sheet> gViews;
	bool gReady = false;

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

	std::wstring SheetsDir(const std::wstring& addonDir)
	{
		return addonDir + L"\\cheatsheets";
	}

	bool ReadFileUtf8(const std::wstring& path, std::string& out)
	{
		HANDLE in = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (in == INVALID_HANDLE_VALUE)
			return false;
		LARGE_INTEGER li{};
		if (!GetFileSizeEx(in, &li) || li.QuadPart <= 0 || li.QuadPart > 8 * 1024 * 1024)
		{
			CloseHandle(in);
			return false;
		}
		out.assign(static_cast<size_t>(li.QuadPart), '\0');
		DWORD got = 0;
		const BOOL ok = ReadFile(in, out.data(), static_cast<DWORD>(out.size()), &got, nullptr);
		CloseHandle(in);
		return ok && got == out.size();
	}

	bool WriteBytes(const std::wstring& path, const void* data, DWORD len)
	{
		HANDLE out = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
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

	bool ExtractPack(const std::wstring& addonDir)
	{
		const unsigned char* begin = _binary_cheatsheets_zip_start;
		const unsigned char* end = _binary_cheatsheets_zip_end;
		if (end <= begin)
			return false;
		const size_t size = static_cast<size_t>(end - begin);

		const std::wstring dir = SheetsDir(addonDir);
		const std::wstring verPath = dir + L"\\cheatsheets.ver";
		const std::wstring manifestPath = dir + L"\\manifest.json";

		if (StampMatches(verPath) &&
			GetFileAttributesW(manifestPath.c_str()) != INVALID_FILE_ATTRIBUTES &&
			GetFileAttributesW((dir + L"\\shared.css").c_str()) != INVALID_FILE_ATTRIBUTES)
			return true;

		CreateDirectoryW(addonDir.c_str(), nullptr);
		CreateDirectoryW(dir.c_str(), nullptr);

		mz_zip_archive zip{};
		if (!mz_zip_reader_init_mem(&zip, begin, size, 0))
			return false;

		bool ok = true;
		const mz_uint n = mz_zip_reader_get_num_files(&zip);
		for (mz_uint i = 0; i < n; ++i)
		{
			mz_zip_archive_file_stat st{};
			if (!mz_zip_reader_file_stat(&zip, i, &st) || st.m_is_directory)
				continue;
			std::string name = st.m_filename;
			if (name.empty() || name.find("..") != std::string::npos)
				continue;
			/* Nested zip paths are skipped — do not abort the whole pack. */
			if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos)
				continue;
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
			return false;
		if (GetFileAttributesW(manifestPath.c_str()) == INVALID_FILE_ATTRIBUTES)
			return false;

		return WriteBytes(verPath, kPackStamp, static_cast<DWORD>(std::strlen(kPackStamp)));
	}

	/* Minimal JSON string field extractor for our manifest shape. */
	bool JsonStringField(const std::string& obj, const char* key, std::string& out)
	{
		const std::string needle = std::string("\"") + key + "\"";
		size_t p = obj.find(needle);
		if (p == std::string::npos)
			return false;
		p = obj.find(':', p + needle.size());
		if (p == std::string::npos)
			return false;
		++p;
		while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' || obj[p] == '\n' || obj[p] == '\r'))
			++p;
		if (p >= obj.size() || obj[p] != '"')
			return false;
		++p;
		out.clear();
		while (p < obj.size())
		{
			const char c = obj[p++];
			if (c == '"')
				return true;
			if (c == '\\' && p < obj.size())
			{
				const char e = obj[p++];
				if (e == 'n')
					out.push_back('\n');
				else if (e == 't')
					out.push_back('\t');
				else if (e == 'r')
					out.push_back('\r');
				else
					out.push_back(e);
			}
			else
				out.push_back(c);
		}
		return false;
	}

	bool ParseManifest(const std::string& json)
	{
		gOwned.clear();
		gViews.clear();
		size_t sheets = json.find("\"sheets\"");
		if (sheets == std::string::npos)
			return false;
		size_t arr = json.find('[', sheets);
		if (arr == std::string::npos)
			return false;
		size_t i = arr + 1;
		while (i < json.size())
		{
			while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\n' ||
				json[i] == '\r' || json[i] == ','))
				++i;
			if (i >= json.size() || json[i] == ']')
				break;
			if (json[i] != '{')
				return false;
			const size_t obj0 = i;
			int depth = 0;
			do
			{
				if (json[i] == '{')
					++depth;
				else if (json[i] == '}')
					--depth;
				else if (json[i] == '"')
				{
					++i;
					while (i < json.size())
					{
						if (json[i] == '\\')
						{
							i += 2;
							continue;
						}
						if (json[i] == '"')
						{
							++i;
							break;
						}
						++i;
					}
					continue;
				}
				++i;
			} while (i < json.size() && depth > 0);
			if (depth != 0)
				return false;
			const std::string obj = json.substr(obj0, i - obj0);

			OwnedSheet row;
			std::string file;
			if (!JsonStringField(obj, "id", row.id) ||
				!JsonStringField(obj, "about", row.about) ||
				!JsonStringField(obj, "file", file) ||
				!JsonStringField(obj, "version", row.version) ||
				!JsonStringField(obj, "browseLabel", row.browseLabel) ||
				!JsonStringField(obj, "browseTitle", row.browseTitle))
				return false;
			if (file.size() > 5 && file.compare(file.size() - 5, 5, ".html") == 0)
				row.fileStem = file.substr(0, file.size() - 5);
			else
				row.fileStem = file;

			row.view.id = row.id.c_str();
			row.view.about = row.about.c_str();
			row.view.fileStem = row.fileStem.c_str();
			row.view.version = row.version.c_str();
			row.view.browseLabel = row.browseLabel.c_str();
			row.view.browseTitle = row.browseTitle.c_str();
			gOwned.push_back(std::move(row));
		}

		gViews.reserve(gOwned.size());
		for (OwnedSheet& o : gOwned)
		{
			/* Re-bind after move into vector (c_str from stable string members). */
			o.view.id = o.id.c_str();
			o.view.about = o.about.c_str();
			o.view.fileStem = o.fileStem.c_str();
			o.view.version = o.version.c_str();
			o.view.browseLabel = o.browseLabel.c_str();
			o.view.browseTitle = o.browseTitle.c_str();
			gViews.push_back(o.view);
		}
		return !gViews.empty();
	}

	void PatchSharedCssUserTheme(const std::wstring& addonDir)
	{
		const std::wstring cssPath = SheetsDir(addonDir) + L"\\shared.css";
		std::string css;
		if (!ReadFileUtf8(cssPath, css))
			return;
		const size_t begin = css.find("/* user-theme begin */");
		if (begin != std::string::npos)
		{
			const size_t end = css.find("/* user-theme end */", begin);
			if (end != std::string::npos)
				css.erase(begin, end + 20 - begin);
		}
		const std::string& ut = UserTheme::CssRootOverride();
		if (!ut.empty())
		{
			css += "\n/* user-theme begin */\n";
			css += ut;
			css += "\n/* user-theme end */\n";
		}
		WriteBytes(cssPath, css.data(), static_cast<DWORD>(css.size()));
	}

	bool EnsureCatalog()
	{
		if (gReady)
			return true;
		const std::wstring addonDir = AddonPaths::DataDir();
		if (addonDir.empty())
			return false;
		if (!ExtractPack(addonDir))
		{
			if (G::API && G::API->Log)
				G::API->Log(LOGL_WARNING, ADDON_NAME, "cheatsheets pack extract failed");
			return false;
		}
		/* Layer Immersive panel fill onto shared.css (absolute file URL). */
		{
			const std::wstring cssPath = SheetsDir(addonDir) + L"\\shared.css";
			std::string css;
			if (ReadFileUtf8(cssPath, css))
			{
				const std::string fill = UiChrome::FillFileUrl(addonDir);
				const std::string fillCss = HelperThemeCss::FillBackgroundCss(fill.c_str());
				const std::string decorCss = UiChrome::DecorCss(addonDir);
				if ((!fillCss.empty() || !decorCss.empty()) &&
					css.find("/* ui-chrome decor */") == std::string::npos &&
					css.find("background-image: url(\"file") == std::string::npos)
				{
					css += "\n/* ui-chrome fill */\n";
					css += fillCss;
					css += decorCss;
					WriteBytes(cssPath, css.data(), static_cast<DWORD>(css.size()));
				}
			}
		}
		PatchSharedCssUserTheme(addonDir);
		std::string json;
		if (!ReadFileUtf8(SheetsDir(addonDir) + L"\\manifest.json", json) || !ParseManifest(json))
		{
			if (G::API && G::API->Log)
				G::API->Log(LOGL_WARNING, ADDON_NAME, "cheatsheets manifest parse failed");
			return false;
		}
		gReady = true;
		if (G::API && G::API->Log)
		{
			char buf[96];
			std::snprintf(buf, sizeof(buf), "Loaded cheatsheets (%zu pages)", gViews.size());
			G::API->Log(LOGL_INFO, ADDON_NAME, buf);
		}
		return true;
	}
} // namespace

void CheatSheets::RefreshUserThemeCss()
{
	/* Never extract from Settings — only rewrite if pack already on disk. */
	const std::wstring addonDir = AddonPaths::DataDir();
	if (addonDir.empty())
		return;
	const std::wstring cssPath = SheetsDir(addonDir) + L"\\shared.css";
	if (GetFileAttributesW(cssPath.c_str()) == INVALID_FILE_ATTRIBUTES)
		return;
	PatchSharedCssUserTheme(addonDir);
}

const CheatSheets::Sheet* CheatSheets::All(size_t* outCount)
{
	(void)EnsureCatalog();
	if (outCount)
		*outCount = gViews.size();
	return gViews.empty() ? nullptr : gViews.data();
}

const CheatSheets::Sheet* CheatSheets::FindByAbout(const char* aboutUrl)
{
	if (!aboutUrl || !aboutUrl[0])
		return nullptr;
	(void)EnsureCatalog();
	for (const Sheet& s : gViews)
	{
		if (s.about && std::strcmp(s.about, aboutUrl) == 0)
			return &s;
	}
	return nullptr;
}

std::string CheatSheets::EnsureFileUrl(const std::wstring& addonDir, const Sheet& sheet)
{
	if (addonDir.empty() || !sheet.fileStem || !sheet.fileStem[0])
		return {};
	if (!EnsureCatalog())
		return {};

	const std::wstring path = SheetsDir(addonDir) + L"\\" +
		std::wstring(sheet.fileStem, sheet.fileStem + std::strlen(sheet.fileStem)) + L".html";
	std::string html;
	if (!ReadFileUtf8(path, html))
	{
		gReady = false;
		if (!ExtractPack(addonDir) || !EnsureCatalog() || !ReadFileUtf8(path, html))
			return {};
	}
	/* CEF OSR viewport height for short-page vertical centering. */
	if (html.find("--app-h") == std::string::npos)
	{
		const size_t bodyClose = html.rfind("</body>");
		if (bodyClose != std::string::npos)
		{
			html.insert(bodyClose, HelperThemeCss::ViewportSyncJs());
			WriteBytes(path, html.data(), static_cast<DWORD>(html.size()));
		}
	}
	return PathToFileUrl(path);
}

std::string CheatSheets::ResolveAboutUrl(const std::wstring& addonDir, const std::string& url)
{
	const Sheet* sheet = FindByAbout(url.c_str());
	if (!sheet)
		return {};
	return EnsureFileUrl(addonDir, *sheet);
}
