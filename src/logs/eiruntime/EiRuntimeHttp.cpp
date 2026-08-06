#include "EiRuntime.h"
#include "EiRuntimeInternal.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>
#include <winhttp.h>

namespace EiRuntimeDetail
{
	bool DownloadToFile(const wchar_t* urlW, const std::wstring& outPath, void (*statusFn)(const char*))
	{
		Status(statusFn, "Downloading Elite Insights...");
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
						std::snprintf(msg, sizeof(msg), "Downloading Elite Insights... %lu%%",
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

	bool ParseLatestCliRelease(const std::string& json, LatestRelease& out)
	{
		out = {};
		std::string tag;
		if (!JsonStringAfterKey(json.c_str(), "tag_name", tag))
			return false;
		out.stamp = NormalizeTag(tag);

		/* Find the GW2EICLI.zip asset object - search for the filename then walk back. */
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
		Status(statusFn, "Checking latest Elite Insights...");
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

} // namespace EiRuntimeDetail
