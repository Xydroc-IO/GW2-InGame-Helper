#include "EiRuntime.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>

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

	bool FindCliRecursive(const std::wstring& dir, std::wstring& out, int depth)
	{
		if (dir.empty() || depth > 8)
			return false;
		const std::wstring direct = Join(dir, L"GuildWars2EliteInsights-CLI.exe");
		if (GetFileAttributesW(direct.c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			out = direct;
			return true;
		}

		WIN32_FIND_DATAW fd{};
		const std::wstring glob = Join(dir, L"*");
		HANDLE h = FindFirstFileW(glob.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		bool found = false;
		do
		{
			if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
				continue;
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				continue;
			if (FindCliRecursive(Join(dir, fd.cFileName), out, depth + 1))
			{
				found = true;
				break;
			}
		} while (FindNextFileW(h, &fd));
		FindClose(h);
		return found;
	}

	bool TreeLooksComplete(const std::wstring& eiDir)
	{
		std::wstring cli;
		return FindCliRecursive(eiDir, cli, 0);
	}

	bool AlreadyInstalled(const std::wstring& eiDir)
	{
		if (!TreeLooksComplete(eiDir))
			return false;
		std::string stamp;
		return ReadStamp(Join(eiDir, L"ei.ver"), stamp) && !stamp.empty();
	}

	bool MatchesStamp(const std::wstring& eiDir, const char* stamp)
	{
		if (!stamp || !stamp[0] || !TreeLooksComplete(eiDir))
			return false;
		std::string cur;
		if (!ReadStamp(Join(eiDir, L"ei.ver"), cur))
			return false;
		return cur == stamp;
	}

	std::string NormalizeTag(const std::string& tag)
	{
		if (tag.size() >= 2 && (tag[0] == 'v' || tag[0] == 'V') &&
			tag[1] >= '0' && tag[1] <= '9')
			return tag.substr(1);
		return tag;
	}

	std::wstring DllDir()
	{
		HMODULE mod = nullptr;
		if (!GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&EiRuntime::EnsureInstalled), &mod) ||
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

	void CleanupStrayZipInEi(const std::wstring& eiDir)
	{
		DeleteFileW(Join(eiDir, L"GW2EICLI.zip").c_str());
		DeleteFileW(Join(eiDir, L"ei-download.tmp").c_str());
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

	bool VerifySha256(const std::wstring& path, const char* sha256Hex, void (*statusFn)(const char*))
	{
		if (!sha256Hex || !sha256Hex[0])
		{
			Status(statusFn, "EI SHA256 missing");
			return false;
		}
		unsigned char expect[32]{};
		if (!ParseSha256Hex(sha256Hex, expect))
		{
			Status(statusFn, "EI SHA256 is invalid");
			return false;
		}
		Status(statusFn, "Verifying Elite Insights…");
		unsigned char got[32]{};
		if (!Sha256File(path, got))
		{
			Status(statusFn, "EI hash read failed");
			return false;
		}
		if (std::memcmp(got, expect, 32) != 0)
		{
			Status(statusFn, "EI SHA256 mismatch");
			return false;
		}
		return true;
	}

	bool ExtractZip(const std::wstring& zipPath, const std::wstring& destDir, void (*statusFn)(const char*))
	{
		Status(statusFn, "Extracting Elite Insights…");
		const std::string zipUtf8 = WideToUtf8(zipPath);

		mz_zip_archive zip{};
		if (!mz_zip_reader_init_file(&zip, zipUtf8.c_str(), 0))
		{
			Status(statusFn, "EI zip open failed");
			return false;
		}

		const std::wstring staging = destDir + L".staging";
		RemoveTree(staging);
		if (!MkDirDeep(staging))
		{
			mz_zip_reader_end(&zip);
			Status(statusFn, "EI staging mkdir failed");
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
			Status(statusFn, "EI extract failed");
			return false;
		}
		if (!TreeLooksComplete(staging))
		{
			RemoveTree(staging);
			Status(statusFn, "EI zip missing CLI exe");
			return false;
		}

		RemoveTree(destDir);
		if (!MoveFileW(staging.c_str(), destDir.c_str()))
		{
			Status(statusFn, "EI install move failed");
			RemoveTree(staging);
			return false;
		}
		return true;
	}

	bool DownloadToFile(const wchar_t* urlW, const std::wstring& outPath, void (*statusFn)(const char*))
	{
		Status(statusFn, "Downloading Elite Insights…");
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
			Status(statusFn, "EI URL parse failed");
			return false;
		}

		HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/EI",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session)
		{
			Status(statusFn, "EI WinHttpOpen failed");
			return false;
		}

		const INTERNET_PORT port = uc.nPort ? uc.nPort :
			(uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT
				: INTERNET_DEFAULT_HTTP_PORT);
		HINTERNET conn = WinHttpConnect(session, host, port, 0);
		if (!conn)
		{
			WinHttpCloseHandle(session);
			Status(statusFn, "EI WinHttpConnect failed");
			return false;
		}

		DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
		HINTERNET req = WinHttpOpenRequest(conn, L"GET", path, nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
		if (!req)
		{
			WinHttpCloseHandle(conn);
			WinHttpCloseHandle(session);
			Status(statusFn, "EI WinHttpOpenRequest failed");
			return false;
		}

		DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
		WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));

		bool ok = false;
		if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
				WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
			!WinHttpReceiveResponse(req, nullptr))
		{
			Status(statusFn, "EI download request failed");
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
				std::snprintf(buf, sizeof(buf), "EI download HTTP %lu",
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
				Status(statusFn, "EI temp CreateFile failed");
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
					Status(statusFn, "EI download query failed");
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
					Status(statusFn, "EI download read failed");
					goto done;
				}
				DWORD written = 0;
				if (!WriteFile(out, buf.data(), got, &written, nullptr) || written != got)
				{
					CloseHandle(out);
					DeleteFileW(outPath.c_str());
					Status(statusFn, "EI download write failed");
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
						std::snprintf(msg, sizeof(msg), "Downloading Elite Insights… %lu%%",
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
				Status(statusFn, "EI download empty");
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

	bool InstallFromZip(const std::wstring& zipPath, const std::wstring& eiDir,
		const char* stamp, const char* sha256Hex, void (*statusFn)(const char*), bool deleteZipAfter)
	{
		if (!stamp || !stamp[0])
		{
			Status(statusFn, "EI stamp missing");
			return false;
		}
		if (!VerifySha256(zipPath, sha256Hex, statusFn))
			return false;
		if (!ExtractZip(zipPath, eiDir, statusFn))
			return false;
		if (!WriteStamp(Join(eiDir, L"ei.ver"), stamp))
		{
			Status(statusFn, "EI stamp write failed");
			return false;
		}
		CleanupStrayZipInEi(eiDir);
		if (deleteZipAfter)
			DeleteFileW(zipPath.c_str());
		if (!MatchesStamp(eiDir, stamp))
		{
			Status(statusFn, "EI install incomplete");
			return false;
		}
		char msg[96];
		std::snprintf(msg, sizeof(msg), "Elite Insights %s ready", stamp);
		Status(statusFn, msg);
		return true;
	}

	bool HttpGetUtf8(const char* urlUtf8, std::string& body, void (*statusFn)(const char*))
	{
		body.clear();
		if (!urlUtf8 || !urlUtf8[0])
			return false;

		int n = MultiByteToWideChar(CP_UTF8, 0, urlUtf8, -1, nullptr, 0);
		if (n <= 0)
			return false;
		std::wstring urlW(static_cast<size_t>(n - 1), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, urlUtf8, -1, urlW.data(), n);

		URL_COMPONENTSW uc{};
		uc.dwStructSize = sizeof(uc);
		wchar_t host[256]{};
		wchar_t path[2048]{};
		uc.lpszHostName = host;
		uc.dwHostNameLength = 256;
		uc.lpszUrlPath = path;
		uc.dwUrlPathLength = 2048;
		if (!WinHttpCrackUrl(urlW.c_str(), 0, 0, &uc))
			return false;

		HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/EI",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session)
			return false;
		WinHttpSetTimeouts(session, 8000, 8000, 8000, 15000);

		const INTERNET_PORT port = uc.nPort ? uc.nPort :
			(uc.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_DEFAULT_HTTPS_PORT
				: INTERNET_DEFAULT_HTTP_PORT);
		HINTERNET conn = WinHttpConnect(session, host, port, 0);
		if (!conn)
		{
			WinHttpCloseHandle(session);
			return false;
		}

		DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
		HINTERNET req = WinHttpOpenRequest(conn, L"GET", path, nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
		if (!req)
		{
			WinHttpCloseHandle(conn);
			WinHttpCloseHandle(session);
			return false;
		}

		DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
		WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));

		const wchar_t* headers =
			L"Accept: application/vnd.github+json\r\n"
			L"User-Agent: GW2-InGame-Helper\r\n"
			L"X-GitHub-Api-Version: 2022-11-28\r\n";

		bool ok = false;
		if (!WinHttpSendRequest(req, headers, static_cast<DWORD>(-1),
				WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
			!WinHttpReceiveResponse(req, nullptr))
		{
			Status(statusFn, "EI release check failed");
			goto done;
		}

		{
			DWORD statusCode = 0;
			DWORD statusSize = sizeof(statusCode);
			WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
			if (statusCode != 200)
			{
				char buf[80];
				std::snprintf(buf, sizeof(buf), "EI release API HTTP %lu",
					static_cast<unsigned long>(statusCode));
				Status(statusFn, buf);
				goto done;
			}
		}

		{
			char chunk[4096];
			DWORD got = 0;
			while (WinHttpReadData(req, chunk, sizeof(chunk), &got) && got > 0)
			{
				body.append(chunk, got);
				if (body.size() > 2 * 1024 * 1024)
				{
					body.clear();
					Status(statusFn, "EI release API response too large");
					goto done;
				}
			}
			ok = !body.empty();
		}

	done:
		WinHttpCloseHandle(req);
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);
		return ok;
	}

	bool JsonStringAfterKey(const char* json, const char* key, std::string& out)
	{
		out.clear();
		if (!json || !key)
			return false;
		char pat[96];
		std::snprintf(pat, sizeof(pat), "\"%s\"", key);
		const char* p = std::strstr(json, pat);
		if (!p)
			return false;
		p += std::strlen(pat);
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
			++p;
		if (*p != ':')
			return false;
		++p;
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
			++p;
		if (*p != '"')
			return false;
		++p;
		while (*p && *p != '"')
		{
			if (*p == '\\' && p[1])
			{
				++p;
				out.push_back(*p);
			}
			else
				out.push_back(*p);
			++p;
		}
		return !out.empty();
	}

	struct LatestRelease
	{
		std::string stamp;
		std::string url;
		std::string sha256;
	};

	bool ParseLatestCliRelease(const std::string& json, LatestRelease& out)
	{
		out = {};
		std::string tag;
		if (!JsonStringAfterKey(json.c_str(), "tag_name", tag))
			return false;
		out.stamp = NormalizeTag(tag);

		/* Find the GW2EICLI.zip asset object — search for the filename then walk back. */
		const char* asset = std::strstr(json.c_str(), "\"name\":\"GW2EICLI.zip\"");
		if (!asset)
			asset = std::strstr(json.c_str(), "\"name\": \"GW2EICLI.zip\"");
		if (!asset)
			return false;

		/* Walk backward to the start of this asset object. */
		const char* obj = asset;
		while (obj > json.c_str() && *obj != '{')
			--obj;
		if (*obj != '{')
			return false;

		/* Bound the object roughly by finding the next digest/url within a window. */
		const char* end = std::strstr(obj, "\"GW2EICLI.zip.sig\"");
		if (!end)
			end = obj + std::strlen(obj);
		std::string slice(obj, end);

		if (!JsonStringAfterKey(slice.c_str(), "browser_download_url", out.url))
			return false;

		std::string digest;
		if (JsonStringAfterKey(slice.c_str(), "digest", digest))
		{
			constexpr const char* kPref = "sha256:";
			if (digest.rfind(kPref, 0) == 0)
				out.sha256 = digest.substr(std::strlen(kPref));
			else if (digest.size() == 64)
				out.sha256 = digest;
		}
		return !out.stamp.empty() && !out.url.empty() && out.sha256.size() == 64;
	}

	bool QueryLatestRelease(LatestRelease& out, void (*statusFn)(const char*))
	{
		Status(statusFn, "Checking latest Elite Insights…");
		std::string body;
		if (!HttpGetUtf8(EiRuntime::kLatestApiUrl, body, statusFn))
			return false;
		if (!ParseLatestCliRelease(body, out))
		{
			Status(statusFn, "EI release parse failed");
			return false;
		}
		char msg[96];
		std::snprintf(msg, sizeof(msg), "Latest Elite Insights: %s", out.stamp.c_str());
		Status(statusFn, msg);
		return true;
	}

	bool FindLocalZip(const std::wstring& addonDir, std::wstring& outPath, bool& deleteAfter)
	{
		outPath.clear();
		deleteAfter = false;
		const wchar_t* name = L"GW2EICLI.zip";

		const std::wstring dllSide = Join(DllDir(), name);
		const std::wstring addonSide = Join(addonDir, name);
		const std::wstring insideEi = Join(Join(addonDir, L"ei"), name);
		const std::wstring candidates[] = { addonSide, dllSide, insideEi };

		for (const std::wstring& c : candidates)
		{
			if (c.empty())
				continue;
			if (GetFileAttributesW(c.c_str()) == INVALID_FILE_ATTRIBUTES)
				continue;
			outPath = c;
			deleteAfter = (c == insideEi);
			return true;
		}
		return false;
	}
}

bool EiRuntime::IsInstalled(const wchar_t* addonDirWide)
{
	if (!addonDirWide || !addonDirWide[0])
		return false;
	return AlreadyInstalled(Join(addonDirWide, L"ei"));
}

bool EiRuntime::GetInstalledStamp(const wchar_t* addonDirWide, char* out, size_t outLen)
{
	if (!out || outLen == 0)
		return false;
	out[0] = 0;
	if (!addonDirWide || !addonDirWide[0])
		return false;
	std::string stamp;
	if (!ReadStamp(Join(Join(addonDirWide, L"ei"), L"ei.ver"), stamp) || stamp.empty())
		return false;
	if (stamp.size() >= outLen)
		return false;
	std::memcpy(out, stamp.c_str(), stamp.size() + 1);
	return true;
}

bool EiRuntime::GetCliPathUtf8(const wchar_t* addonDirWide, char* out, size_t outLen)
{
	if (!out || outLen == 0)
		return false;
	out[0] = 0;
	if (!addonDirWide || !addonDirWide[0])
		return false;
	const std::wstring eiDir = Join(addonDirWide, L"ei");
	std::wstring cli;
	if (!FindCliRecursive(eiDir, cli, 0))
		return false;
	const std::string utf8 = WideToUtf8(cli);
	if (utf8.empty() || utf8.size() >= outLen)
		return false;
	std::memcpy(out, utf8.c_str(), utf8.size() + 1);
	return true;
}

bool EiRuntime::EnsureInstalled(const wchar_t* addonDirWide, void (*statusFn)(const char* msg))
{
	if (!addonDirWide || !addonDirWide[0])
	{
		Status(statusFn, "EI addon dir missing");
		return false;
	}

	const std::wstring addonDir = addonDirWide;
	const std::wstring eiDir = Join(addonDir, L"ei");

	if (!MkDirDeep(addonDir))
	{
		Status(statusFn, "EI addon mkdir failed");
		return false;
	}

	LatestRelease latest;
	const bool haveLatest = QueryLatestRelease(latest, statusFn);
	if (!haveLatest)
	{
		/* Offline / API down: keep current install, else fallback pin. */
		if (AlreadyInstalled(eiDir))
		{
			CleanupStrayZipInEi(eiDir);
			Status(statusFn, "Elite Insights ready (offline — kept current)");
			return true;
		}

		latest.stamp = kFallbackStamp;
		latest.url = kFallbackDownloadUrl;
		latest.sha256 = kFallbackSha256Hex;
		Status(statusFn, "Using fallback Elite Insights package…");
	}
	else if (MatchesStamp(eiDir, latest.stamp.c_str()))
	{
		CleanupStrayZipInEi(eiDir);
		char msg[96];
		std::snprintf(msg, sizeof(msg), "Elite Insights %s up to date", latest.stamp.c_str());
		Status(statusFn, msg);
		return true;
	}

	std::wstring localZip;
	bool deleteAfter = false;
	if (FindLocalZip(addonDir, localZip, deleteAfter))
	{
		Status(statusFn, "Installing Elite Insights from local zip…");
		if (InstallFromZip(localZip, eiDir, latest.stamp.c_str(), latest.sha256.c_str(),
				statusFn, deleteAfter))
			return true;
		Status(statusFn, "Local EI zip did not match latest hash — downloading…");
	}

	if (latest.url.empty())
	{
		Status(statusFn, "EI zip missing — place GW2EICLI.zip next to the DLL");
		return false;
	}

	int n = MultiByteToWideChar(CP_UTF8, 0, latest.url.c_str(), -1, nullptr, 0);
	if (n <= 0)
	{
		Status(statusFn, "EI URL widen failed");
		return false;
	}
	std::wstring urlW(static_cast<size_t>(n - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, latest.url.c_str(), -1, urlW.data(), n);

	const std::wstring tmpZip = Join(addonDir, L"ei-download.tmp");
	DeleteFileW(tmpZip.c_str());

	if (!DownloadToFile(urlW.c_str(), tmpZip, statusFn))
		return false;

	const bool ok = InstallFromZip(tmpZip, eiDir, latest.stamp.c_str(), latest.sha256.c_str(),
		statusFn, true);
	if (!ok)
		DeleteFileW(tmpZip.c_str());
	return ok;
}

namespace
{
	bool gDotNetCacheValid = false;
	bool gDotNetCacheValue = false;
	DWORD gDotNetCacheMs = 0;

	bool SharedFxHasV8(const std::wstring& fxDir)
	{
		if (fxDir.empty())
			return false;
		WIN32_FIND_DATAW fd{};
		const std::wstring pat = fxDir + L"\\8.*";
		HANDLE h = FindFirstFileW(pat.c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE)
			return false;
		bool ok = false;
		do
		{
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				ok = true;
				break;
			}
		} while (FindNextFileW(h, &fd));
		FindClose(h);
		return ok;
	}

	bool DotNetRootHasV8(const std::wstring& root)
	{
		if (root.empty())
			return false;
		if (SharedFxHasV8(root + L"\\shared\\Microsoft.NETCore.App"))
			return true;
		if (SharedFxHasV8(root + L"\\shared\\Microsoft.WindowsDesktop.App"))
			return true;
		return false;
	}

	bool RegistryHasDotNet8()
	{
		const wchar_t* keys[] = {
			L"SOFTWARE\\dotnet\\Setup\\InstalledVersions\\x64\\sharedfx\\Microsoft.NETCore.App",
			L"SOFTWARE\\dotnet\\Setup\\InstalledVersions\\x64\\sharedfx\\Microsoft.WindowsDesktop.App",
			L"SOFTWARE\\WOW6432Node\\dotnet\\Setup\\InstalledVersions\\x64\\sharedfx\\Microsoft.NETCore.App",
			L"SOFTWARE\\WOW6432Node\\dotnet\\Setup\\InstalledVersions\\x64\\sharedfx\\Microsoft.WindowsDesktop.App",
		};
		for (const wchar_t* sub : keys)
		{
			HKEY hk = nullptr;
			if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, sub, 0, KEY_READ, &hk) != ERROR_SUCCESS)
				continue;
			DWORD index = 0;
			wchar_t name[128];
			DWORD nameLen = 128;
			bool found = false;
			while (RegEnumValueW(hk, index++, name, &nameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
			{
				if (name[0] == L'8' && name[1] == L'.')
				{
					found = true;
					break;
				}
				nameLen = 128;
			}
			RegCloseKey(hk);
			if (found)
				return true;
		}
		return false;
	}

	bool ProbeDotNet8()
	{
		wchar_t pf[MAX_PATH]{};
		if (GetEnvironmentVariableW(L"ProgramFiles", pf, MAX_PATH) > 0 &&
			DotNetRootHasV8(std::wstring(pf) + L"\\dotnet"))
			return true;

		wchar_t pf86[MAX_PATH]{};
		if (GetEnvironmentVariableW(L"ProgramFiles(x86)", pf86, MAX_PATH) > 0 &&
			DotNetRootHasV8(std::wstring(pf86) + L"\\dotnet"))
			return true;

		wchar_t root[MAX_PATH]{};
		if (GetEnvironmentVariableW(L"DOTNET_ROOT", root, MAX_PATH) > 0 && DotNetRootHasV8(root))
			return true;

		wchar_t local[MAX_PATH]{};
		if (GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH) > 0 &&
			DotNetRootHasV8(std::wstring(local) + L"\\Microsoft\\dotnet"))
			return true;

		if (RegistryHasDotNet8())
			return true;

		return false;
	}
}

bool EiRuntime::IsWine()
{
	static int sCached = -1;
	if (sCached >= 0)
		return sCached != 0;
	HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	sCached = (ntdll && GetProcAddress(ntdll, "wine_get_version")) ? 1 : 0;
	return sCached != 0;
}

void EiRuntime::InvalidateDotNet8Cache()
{
	gDotNetCacheValid = false;
	gDotNetCacheMs = 0;
}

bool EiRuntime::HasDotNet8Runtime()
{
	const DWORD now = GetTickCount();
	if (gDotNetCacheValid && (now - gDotNetCacheMs) < 5000u)
		return gDotNetCacheValue;
	gDotNetCacheValue = ProbeDotNet8();
	gDotNetCacheMs = now;
	gDotNetCacheValid = true;
	return gDotNetCacheValue;
}

void EiRuntime::OpenDotNet8Installer()
{
	/* Direct x64 Desktop Runtime installer — works on Windows and in Wine/Proton prefixes. */
	ShellExecuteA(nullptr, "open",
		"https://aka.ms/dotnet/8.0/windowsdesktop-runtime-win-x64.exe",
		nullptr, nullptr, SW_SHOWNORMAL);
}
