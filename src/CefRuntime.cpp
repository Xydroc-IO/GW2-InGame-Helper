#include "CefRuntime.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#include "miniz.h"

namespace
{
	void Status(void (*fn)(const char*), const char* msg)
	{
		if (fn && msg)
			fn(msg);
	}

	std::wstring Join(const std::wstring& a, const wchar_t* b)
	{
		if (a.empty())
			return b ? b : L"";
		if (!b || !b[0])
			return a;
		if (a.back() == L'\\' || a.back() == L'/')
			return a + b;
		return a + L"\\" + b;
	}

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

	bool ReadStamp(const std::wstring& verPath, std::string& out)
	{
		out.clear();
		HANDLE h = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		char buf[64]{};
		DWORD got = 0;
		const BOOL ok = ReadFile(h, buf, sizeof(buf) - 1, &got, nullptr);
		CloseHandle(h);
		if (!ok || got == 0)
			return false;
		out.assign(buf, got);
		while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' '))
			out.pop_back();
		return true;
	}

	bool WriteStamp(const std::wstring& verPath, const char* stamp)
	{
		HANDLE h = CreateFileW(verPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(h, stamp, static_cast<DWORD>(std::strlen(stamp)), &written, nullptr);
		CloseHandle(h);
		return ok && written == std::strlen(stamp);
	}

	bool MkDirDeep(const std::wstring& path)
	{
		if (path.empty())
			return false;
		if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
			return true;
		const size_t slash = path.find_last_of(L"\\/");
		if (slash != std::wstring::npos)
		{
			if (!MkDirDeep(path.substr(0, slash)))
				return false;
		}
		return CreateDirectoryW(path.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
	}

	void RemoveTree(const std::wstring& path)
	{
		WIN32_FIND_DATAW fd{};
		const std::wstring glob = Join(path, L"*");
		HANDLE h = FindFirstFileW(glob.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
		{
			RemoveDirectoryW(path.c_str());
			DeleteFileW(path.c_str());
			return;
		}
		do
		{
			if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
				continue;
			const std::wstring child = Join(path, fd.cFileName);
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				RemoveTree(child);
			else
				DeleteFileW(child.c_str());
		} while (FindNextFileW(h, &fd));
		FindClose(h);
		RemoveDirectoryW(path.c_str());
	}

	bool TreeLooksComplete(const std::wstring& cefDir)
	{
		if (GetFileAttributesW(Join(cefDir, L"libcef.dll").c_str()) == INVALID_FILE_ATTRIBUTES)
			return false;
		if (GetFileAttributesW(Join(cefDir, L"chrome_elf.dll").c_str()) == INVALID_FILE_ATTRIBUTES)
			return false;
		if (GetFileAttributesW(Join(cefDir, L"icudtl.dat").c_str()) == INVALID_FILE_ATTRIBUTES)
			return false;
		if (GetFileAttributesW(Join(cefDir, L"resources.pak").c_str()) == INVALID_FILE_ATTRIBUTES &&
			GetFileAttributesW(Join(cefDir, L"chrome_100_percent.pak").c_str()) == INVALID_FILE_ATTRIBUTES)
			return false;
		if (GetFileAttributesW(Join(cefDir, L"locales").c_str()) == INVALID_FILE_ATTRIBUTES)
			return false;
		return true;
	}

	bool AlreadyInstalled(const std::wstring& cefDir)
	{
		if (!TreeLooksComplete(cefDir))
			return false;
		std::string stamp;
		if (!ReadStamp(Join(cefDir, L"cef.ver"), stamp))
			return false;
		return stamp == CefRuntime::kStamp;
	}

	std::wstring DllDir()
	{
		HMODULE mod = nullptr;
		if (!GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&CefRuntime::EnsureInstalled), &mod) ||
			!mod)
			return {};
		wchar_t path[MAX_PATH]{};
		if (!GetModuleFileNameW(mod, path, MAX_PATH))
			return {};
		std::wstring full = path;
		const size_t slash = full.find_last_of(L"\\/");
		if (slash == std::wstring::npos)
			return {};
		return full.substr(0, slash);
	}

	void CleanupStrayZipInCef(const std::wstring& cefDir)
	{
		DeleteFileW(Join(cefDir, L"cef-runtime-150-windows64.zip").c_str());
		DeleteFileW(Join(cefDir, L"cef-download.tmp").c_str());
	}

	bool Sha256File(const std::wstring& path, unsigned char out[32])
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
			return false;

		BCRYPT_ALG_HANDLE alg = nullptr;
		BCRYPT_HASH_HANDLE hash = nullptr;
		DWORD objLen = 0, cb = 0;
		bool ok = false;
		std::vector<unsigned char> obj;

		if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
			goto done;
		if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen),
				sizeof(objLen), &cb, 0) != 0)
			goto done;
		obj.resize(objLen);
		if (BCryptCreateHash(alg, &hash, obj.data(), objLen, nullptr, 0, 0) != 0)
			goto done;

		{
			unsigned char buf[1 << 16];
			for (;;)
			{
				DWORD got = 0;
				if (!ReadFile(h, buf, sizeof(buf), &got, nullptr))
					goto done;
				if (got == 0)
					break;
				if (BCryptHashData(hash, buf, got, 0) != 0)
					goto done;
			}
		}
		ok = (BCryptFinishHash(hash, out, 32, 0) == 0);

	done:
		if (hash)
			BCryptDestroyHash(hash);
		if (alg)
			BCryptCloseAlgorithmProvider(alg, 0);
		CloseHandle(h);
		return ok;
	}

	int HexNibble(char c)
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	}

	bool ParseSha256Hex(const char* hex, unsigned char out[32])
	{
		if (!hex || std::strlen(hex) != 64)
			return false;
		for (int i = 0; i < 32; ++i)
		{
			const int hi = HexNibble(hex[i * 2]);
			const int lo = HexNibble(hex[i * 2 + 1]);
			if (hi < 0 || lo < 0)
				return false;
			out[i] = static_cast<unsigned char>((hi << 4) | lo);
		}
		return true;
	}

	bool VerifySha256(const std::wstring& path, void (*statusFn)(const char*))
	{
		if (!CefRuntime::kSha256Hex || !CefRuntime::kSha256Hex[0])
		{
			Status(statusFn, "CEF SHA256 not configured in CefRuntime.h");
			return false;
		}
		unsigned char expect[32]{};
		if (!ParseSha256Hex(CefRuntime::kSha256Hex, expect))
		{
			Status(statusFn, "CEF SHA256 constant is invalid");
			return false;
		}
		Status(statusFn, "Verifying CEF…");
		unsigned char got[32]{};
		if (!Sha256File(path, got))
		{
			Status(statusFn, "CEF hash read failed");
			return false;
		}
		if (std::memcmp(got, expect, 32) != 0)
		{
			Status(statusFn, "CEF SHA256 mismatch");
			return false;
		}
		return true;
	}

	bool ExtractZip(const std::wstring& zipPath, const std::wstring& destDir, void (*statusFn)(const char*))
	{
		Status(statusFn, "Extracting CEF…");
		const std::string zipUtf8 = WideToUtf8(zipPath);

		mz_zip_archive zip{};
		if (!mz_zip_reader_init_file(&zip, zipUtf8.c_str(), 0))
		{
			Status(statusFn, "CEF zip open failed");
			return false;
		}

		const std::wstring staging = destDir + L".staging";
		RemoveTree(staging);
		if (!MkDirDeep(staging))
		{
			mz_zip_reader_end(&zip);
			Status(statusFn, "CEF staging mkdir failed");
			return false;
		}

		const mz_uint n = mz_zip_reader_get_num_files(&zip);
		bool ok = true;
		for (mz_uint i = 0; i < n; ++i)
		{
			mz_zip_archive_file_stat st{};
			if (!mz_zip_reader_file_stat(&zip, i, &st))
			{
				ok = false;
				break;
			}
			if (st.m_is_directory)
			{
				std::wstring dir = staging + L"\\";
				for (const char* p = st.m_filename; *p; ++p)
					dir.push_back(*p == '/' ? L'\\' : static_cast<wchar_t>(static_cast<unsigned char>(*p)));
				if (!MkDirDeep(dir))
				{
					ok = false;
					break;
				}
				continue;
			}

			std::string rel = st.m_filename;
			for (char& c : rel)
			{
				if (c == '/')
					c = '\\';
			}
			if (rel.find("..") != std::string::npos)
			{
				ok = false;
				break;
			}

			const std::wstring outPath = staging + L"\\" + std::wstring(rel.begin(), rel.end());
			const size_t slash = outPath.find_last_of(L"\\/");
			if (slash != std::wstring::npos)
			{
				if (!MkDirDeep(outPath.substr(0, slash)))
				{
					ok = false;
					break;
				}
			}

			const std::string outUtf8 = WideToUtf8(outPath);
			if (!mz_zip_reader_extract_to_file(&zip, i, outUtf8.c_str(), 0))
			{
				ok = false;
				break;
			}
		}
		mz_zip_reader_end(&zip);

		if (!ok)
		{
			RemoveTree(staging);
			Status(statusFn, "CEF extract failed");
			return false;
		}
		if (GetFileAttributesW(Join(staging, L"libcef.dll").c_str()) == INVALID_FILE_ATTRIBUTES)
		{
			RemoveTree(staging);
			Status(statusFn, "CEF zip missing libcef.dll");
			return false;
		}

		RemoveTree(destDir);
		if (!MoveFileW(staging.c_str(), destDir.c_str()))
		{
			Status(statusFn, "CEF install move failed");
			RemoveTree(staging);
			return false;
		}
		return true;
	}

	bool DownloadToFile(const wchar_t* urlW, const std::wstring& outPath, void (*statusFn)(const char*))
	{
		Status(statusFn, "Downloading CEF…");
		URL_COMPONENTSW uc{};
		uc.dwStructSize = sizeof(uc);
		wchar_t host[256]{};
		wchar_t path[2048]{};
		uc.lpszHostName = host;
		uc.dwHostNameLength = 256;
		uc.lpszUrlPath = path;
		uc.dwUrlPathLength = 2048;
		if (!WinHttpCrackUrl(urlW, 0, 0, &uc))
		{
			Status(statusFn, "CEF URL parse failed");
			return false;
		}

		HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper-Beta/CEF",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session)
		{
			Status(statusFn, "CEF WinHttpOpen failed");
			return false;
		}

		const INTERNET_PORT port = uc.nPort ? uc.nPort :
			(uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT
				: INTERNET_DEFAULT_HTTP_PORT);
		HINTERNET conn = WinHttpConnect(session, host, port, 0);
		if (!conn)
		{
			WinHttpCloseHandle(session);
			Status(statusFn, "CEF WinHttpConnect failed");
			return false;
		}

		DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
		HINTERNET req = WinHttpOpenRequest(conn, L"GET", path, nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
		if (!req)
		{
			WinHttpCloseHandle(conn);
			WinHttpCloseHandle(session);
			Status(statusFn, "CEF WinHttpOpenRequest failed");
			return false;
		}

		DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
		WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));

		bool ok = false;
		if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
				WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
			!WinHttpReceiveResponse(req, nullptr))
		{
			Status(statusFn, "CEF download request failed");
			goto done;
		}

		{
			DWORD statusCode = 0;
			DWORD statusSize = sizeof(statusCode);
			WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
			if (statusCode != 200)
			{
				char buf[96];
				std::snprintf(buf, sizeof(buf), "CEF download HTTP %lu",
					static_cast<unsigned long>(statusCode));
				Status(statusFn, buf);
				goto done;
			}
		}

		{
			DWORD contentLen = 0;
			DWORD lenSize = sizeof(contentLen);
			WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &contentLen, &lenSize, WINHTTP_NO_HEADER_INDEX);

			HANDLE out = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL, nullptr);
			if (out == INVALID_HANDLE_VALUE)
			{
				Status(statusFn, "CEF temp CreateFile failed");
				goto done;
			}

			DWORD total = 0;
			DWORD lastPct = 999;
			for (;;)
			{
				DWORD avail = 0;
				if (!WinHttpQueryDataAvailable(req, &avail))
				{
					CloseHandle(out);
					DeleteFileW(outPath.c_str());
					Status(statusFn, "CEF download query failed");
					goto done;
				}
				if (avail == 0)
					break;
				std::vector<char> buf(avail);
				DWORD got = 0;
				if (!WinHttpReadData(req, buf.data(), avail, &got) || got == 0)
				{
					CloseHandle(out);
					DeleteFileW(outPath.c_str());
					Status(statusFn, "CEF download read failed");
					goto done;
				}
				DWORD written = 0;
				if (!WriteFile(out, buf.data(), got, &written, nullptr) || written != got)
				{
					CloseHandle(out);
					DeleteFileW(outPath.c_str());
					Status(statusFn, "CEF download write failed");
					goto done;
				}
				total += got;
				if (contentLen > 0)
				{
					const DWORD pct = static_cast<DWORD>(
						(static_cast<unsigned long long>(total) * 100ull) / contentLen);
					if (pct != lastPct && (pct % 5 == 0 || pct == 100))
					{
						char msg[64];
						std::snprintf(msg, sizeof(msg), "Downloading CEF… %lu%%",
							static_cast<unsigned long>(pct));
						Status(statusFn, msg);
						lastPct = pct;
					}
				}
			}
			CloseHandle(out);
			if (total == 0)
			{
				DeleteFileW(outPath.c_str());
				Status(statusFn, "CEF download empty");
				goto done;
			}
			ok = true;
		}

	done:
		WinHttpCloseHandle(req);
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);
		return ok;
	}

	bool InstallFromZip(const std::wstring& zipPath, const std::wstring& cefDir,
		void (*statusFn)(const char*), bool deleteZipAfter)
	{
		if (!VerifySha256(zipPath, statusFn))
			return false;
		if (!ExtractZip(zipPath, cefDir, statusFn))
			return false;
		if (!WriteStamp(Join(cefDir, L"cef.ver"), CefRuntime::kStamp))
		{
			Status(statusFn, "CEF stamp write failed");
			return false;
		}
		CleanupStrayZipInCef(cefDir);
		if (deleteZipAfter)
			DeleteFileW(zipPath.c_str());
		if (!AlreadyInstalled(cefDir))
		{
			Status(statusFn, "CEF install incomplete");
			return false;
		}
		Status(statusFn, "CEF ready");
		return true;
	}

	bool FindLocalZip(const std::wstring& addonDir, std::wstring& outPath, bool& deleteAfter)
	{
		outPath.clear();
		deleteAfter = false;
		const wchar_t* name = L"cef-runtime-150-windows64.zip";

		const std::wstring dllSide = Join(DllDir(), name);
		const std::wstring addonSide = Join(addonDir, name);
		const std::wstring insideCef = Join(Join(addonDir, L"cef"), name);
		const std::wstring candidates[] = { addonSide, dllSide, insideCef };

		for (const std::wstring& c : candidates)
		{
			if (c.empty())
				continue;
			if (GetFileAttributesW(c.c_str()) == INVALID_FILE_ATTRIBUTES)
				continue;
			outPath = c;
			/* Remove zip only when it was dropped inside cef/ (wrong place). */
			deleteAfter = (c == insideCef);
			return true;
		}
		return false;
	}
}

bool CefRuntime::EnsureInstalled(const wchar_t* addonDirWide, void (*statusFn)(const char* msg))
{
	if (!addonDirWide || !addonDirWide[0])
	{
		Status(statusFn, "CEF addon dir missing");
		return false;
	}

	const std::wstring addonDir = addonDirWide;
	const std::wstring cefDir = Join(addonDir, L"cef");

	if (!MkDirDeep(addonDir))
	{
		Status(statusFn, "CEF addon mkdir failed");
		return false;
	}

	if (AlreadyInstalled(cefDir))
	{
		CleanupStrayZipInCef(cefDir);
		return true;
	}

	/* Never re-hash / re-extract the ~170MB zip just because stamp is missing. */
	if (TreeLooksComplete(cefDir))
	{
		Status(statusFn, "Writing CEF stamp…");
		if (WriteStamp(Join(cefDir, L"cef.ver"), kStamp))
		{
			CleanupStrayZipInCef(cefDir);
			if (AlreadyInstalled(cefDir))
			{
				Status(statusFn, "CEF ready");
				return true;
			}
		}
	}

	std::wstring localZip;
	bool deleteAfter = false;
	if (FindLocalZip(addonDir, localZip, deleteAfter))
	{
		Status(statusFn, "Installing CEF from local zip…");
		return InstallFromZip(localZip, cefDir, statusFn, deleteAfter);
	}

	if (!kDownloadUrl || !kDownloadUrl[0])
	{
		Status(statusFn, "CEF zip missing — place cef-runtime-150-windows64.zip next to the DLL");
		return false;
	}

	int n = MultiByteToWideChar(CP_UTF8, 0, kDownloadUrl, -1, nullptr, 0);
	if (n <= 0)
	{
		Status(statusFn, "CEF URL widen failed");
		return false;
	}
	std::wstring urlW(static_cast<size_t>(n - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, kDownloadUrl, -1, urlW.data(), n);

	const std::wstring tmpZip = Join(addonDir, L"cef-download.tmp");
	DeleteFileW(tmpZip.c_str());

	if (!DownloadToFile(urlW.c_str(), tmpZip, statusFn))
		return false;

	const bool ok = InstallFromZip(tmpZip, cefDir, statusFn, true);
	if (!ok)
		DeleteFileW(tmpZip.c_str());
	return ok;
}
