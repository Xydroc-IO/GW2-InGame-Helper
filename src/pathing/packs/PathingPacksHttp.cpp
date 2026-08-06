#include "PathingPacksInternal.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>
#include <winhttp.h>

namespace PathingPacksDetail
{
bool HttpGetUtf8(const wchar_t* urlW, std::string& body, DWORD timeoutMs)
{
	body.clear();
	URL_COMPONENTSW uc{};
	uc.dwStructSize = sizeof(uc);
	wchar_t host[256]{};
	wchar_t path[2048]{};
	uc.lpszHostName = host;
	uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path;
	uc.dwUrlPathLength = 2048;
	if (!WinHttpCrackUrl(urlW, 0, 0, &uc))
		return false;

	HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/Pathing",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session)
		return false;
	WinHttpSetTimeouts(session, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

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
	if (WinHttpSendRequest(req, headers, static_cast<DWORD>(-1),
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
		WinHttpReceiveResponse(req, nullptr))
	{
		DWORD statusCode = 0;
		DWORD statusSize = sizeof(statusCode);
		WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
		if (statusCode == 200)
		{
			char chunk[8192];
			DWORD got = 0;
			while (WinHttpReadData(req, chunk, sizeof(chunk), &got) && got > 0)
			{
				body.append(chunk, got);
				if (body.size() > 4 * 1024 * 1024)
				{
					body.clear();
					break;
				}
			}
			ok = !body.empty();
		}
	}
	WinHttpCloseHandle(req);
	WinHttpCloseHandle(conn);
	WinHttpCloseHandle(session);
	return ok;
}

bool HttpHeadStamp(const wchar_t* urlW, std::string& stampOut)
{
	stampOut.clear();
	URL_COMPONENTSW uc{};
	uc.dwStructSize = sizeof(uc);
	wchar_t host[256]{};
	wchar_t path[2048]{};
	uc.lpszHostName = host;
	uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path;
	uc.dwUrlPathLength = 2048;
	if (!WinHttpCrackUrl(urlW, 0, 0, &uc))
		return false;

	HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/Pathing",
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
	HINTERNET req = WinHttpOpenRequest(conn, L"HEAD", path, nullptr,
		WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
	if (!req)
	{
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);
		return false;
	}
	DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
	WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));

	bool ok = false;
	if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
		WinHttpReceiveResponse(req, nullptr))
	{
		DWORD statusCode = 0;
		DWORD statusSize = sizeof(statusCode);
		WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
		if (statusCode == 200)
		{
			DWORD contentLen = 0;
			DWORD lenSize = sizeof(contentLen);
			if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
					WINHTTP_HEADER_NAME_BY_INDEX, &contentLen, &lenSize, WINHTTP_NO_HEADER_INDEX) &&
				contentLen > 0)
			{
				char buf[48];
				std::snprintf(buf, sizeof(buf), "len:%lu", static_cast<unsigned long>(contentLen));
				stampOut = buf;
				ok = true;
			}
		}
	}
	WinHttpCloseHandle(req);
	WinHttpCloseHandle(conn);
	WinHttpCloseHandle(session);
	return ok;
}

bool DownloadToFile(const wchar_t* urlW, const std::wstring& outPath)
{
	URL_COMPONENTSW uc{};
	uc.dwStructSize = sizeof(uc);
	wchar_t host[256]{};
	wchar_t path[2048]{};
	uc.lpszHostName = host;
	uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path;
	uc.dwUrlPathLength = 2048;
	if (!WinHttpCrackUrl(urlW, 0, 0, &uc))
		return false;

	HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/Pathing",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session)
		return false;
	/* Large .taco packs (~40-50MB). */
	WinHttpSetTimeouts(session, 15000, 15000, 15000, 600000);

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

	bool ok = false;
	HANDLE file = INVALID_HANDLE_VALUE;
	if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
		WinHttpReceiveResponse(req, nullptr))
	{
		DWORD statusCode = 0;
		DWORD statusSize = sizeof(statusCode);
		WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
		if (statusCode == 200)
		{
			file = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file != INVALID_HANDLE_VALUE)
			{
				std::vector<char> buf(256 * 1024);
				DWORD got = 0;
				ok = true;
				while (WinHttpReadData(req, buf.data(), static_cast<DWORD>(buf.size()), &got) && got > 0)
				{
					if (gCancel.load(std::memory_order_acquire))
					{
						ok = false;
						break;
					}
					DWORD written = 0;
					if (!WriteFile(file, buf.data(), got, &written, nullptr) || written != got)
					{
						ok = false;
						break;
					}
				}
			}
		}
	}
	if (file != INVALID_HANDLE_VALUE)
		CloseHandle(file);
	WinHttpCloseHandle(req);
	WinHttpCloseHandle(conn);
	WinHttpCloseHandle(session);
	if (!ok)
		DeleteFileW(outPath.c_str());
	return ok;
}

} // namespace PathingPacksDetail
