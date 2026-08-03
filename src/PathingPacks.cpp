#include "PathingPacks.h"

#include "Globals.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>
#include <winhttp.h>

namespace
{
	std::atomic<bool> gForceUpdate{false};
	std::atomic<bool> gUpdating{false};
	std::mutex gStatusMu;
	char gStatus[160]{};

	void SetStatus(const char* msg)
	{
		std::lock_guard<std::mutex> lock(gStatusMu);
		if (!msg)
			msg = "";
		std::snprintf(gStatus, sizeof(gStatus), "%s", msg);
		if (msg[0] && G::API && G::API->Log)
			G::API->Log(LOGL_INFO, ADDON_NAME, msg);
	}

	std::wstring Utf8ToWide(const char* u)
	{
		if (!u || !u[0])
			return {};
		int n = MultiByteToWideChar(CP_UTF8, 0, u, -1, nullptr, 0);
		std::wstring out(static_cast<size_t>(n > 0 ? n - 1 : 0), L'\0');
		if (n > 0)
			MultiByteToWideChar(CP_UTF8, 0, u, -1, out.data(), n);
		return out;
	}

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

	bool ReadStamp(const std::wstring& verPath, std::string& out)
	{
		out.clear();
		HANDLE in = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (in == INVALID_HANDLE_VALUE)
			return false;
		char buf[128]{};
		DWORD got = 0;
		if (!ReadFile(in, buf, sizeof(buf) - 1, &got, nullptr))
		{
			CloseHandle(in);
			return false;
		}
		CloseHandle(in);
		while (got > 0 && (buf[got - 1] == '\r' || buf[got - 1] == '\n' || buf[got - 1] == ' '))
			buf[--got] = '\0';
		out.assign(buf, got);
		return !out.empty();
	}

	bool WriteStamp(const std::wstring& verPath, const std::string& stamp)
	{
		HANDLE out = CreateFileW(verPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (out == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(out, stamp.data(), static_cast<DWORD>(stamp.size()), &written, nullptr);
		CloseHandle(out);
		return ok && written == stamp.size();
	}

	bool FileExistsNonEmpty(const std::wstring& path)
	{
		HANDLE in = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (in == INVALID_HANDLE_VALUE)
			return false;
		LARGE_INTEGER li{};
		const bool ok = GetFileSizeEx(in, &li) && li.QuadPart > 1024;
		CloseHandle(in);
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
		/* Large .taco packs (~40–50MB). */
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

	struct CuratedPack
	{
		const char* fileName;     /* on disk under pathing/ */
		const char* displayName;  /* status text */
		const char* githubApi;    /* nullable — releases/latest JSON */
		const char* assetName;    /* GitHub asset name when using API */
		const char* directUrl;    /* fallback / primary for non-GitHub */
	};

	const CuratedPack kPacks[] = {
		{
			"LadyElyssa.taco",
			"Lady Elyssa's Guides",
			"https://api.github.com/repos/LadyElyssa/LadyElyssaTacoTrails/releases/latest",
			"LadyElyssa.taco",
			"https://github.com/LadyElyssa/LadyElyssaTacoTrails/releases/latest/download/LadyElyssa.taco",
		},
		{
			"LadyElyssaAP.taco",
			"Lady Elyssa's Achievements",
			"https://api.github.com/repos/LadyElyssa/LadyElyssaAchievementGuides/releases/latest",
			"LadyElyssaAP.taco",
			"https://github.com/LadyElyssa/LadyElyssaAchievementGuides/releases/latest/download/LadyElyssaAP.taco",
		},
		{
			"tw_ALL_IN_ONE.taco",
			"Tekkit's All-In-One",
			nullptr,
			nullptr,
			"https://www.tekkitsworkshop.net/downloads/tw_ALL_IN_ONE.taco",
		},
	};

	bool ResolveGitHub(const CuratedPack& pack, std::string& urlOut, std::string& stampOut)
	{
		urlOut.clear();
		stampOut.clear();
		if (!pack.githubApi || !pack.assetName)
			return false;
		std::string json;
		const std::wstring apiW = Utf8ToWide(pack.githubApi);
		if (!HttpGetUtf8(apiW.c_str(), json, 15000))
			return false;
		std::string tag;
		if (!JsonStringAfterKey(json.c_str(), "tag_name", tag))
			return false;
		stampOut = tag;

		char namePat[128];
		std::snprintf(namePat, sizeof(namePat), "\"name\":\"%s\"", pack.assetName);
		const char* asset = std::strstr(json.c_str(), namePat);
		if (!asset)
		{
			std::snprintf(namePat, sizeof(namePat), "\"name\": \"%s\"", pack.assetName);
			asset = std::strstr(json.c_str(), namePat);
		}
		if (!asset)
		{
			urlOut = pack.directUrl ? pack.directUrl : "";
			return !urlOut.empty();
		}
		const char* obj = asset;
		while (obj > json.c_str() && *obj != '{')
			--obj;
		const char* end = std::strchr(obj, '}');
		if (!end)
			return false;
		const std::string slice(obj, static_cast<size_t>(end - obj + 1));
		if (!JsonStringAfterKey(slice.c_str(), "browser_download_url", urlOut) || urlOut.empty())
		{
			urlOut = pack.directUrl ? pack.directUrl : "";
			return !urlOut.empty();
		}
		return true;
	}

	bool EnsureOne(const std::wstring& pathingDir, const CuratedPack& pack, bool force)
	{
		const std::wstring filePath = pathingDir + L"\\" + Utf8ToWide(pack.fileName);
		const std::wstring verPath = filePath + L".ver";
		const std::wstring tmpPath = filePath + L".tmp";

		std::string wantStamp;
		std::string url;
		if (pack.githubApi)
		{
			if (!ResolveGitHub(pack, url, wantStamp))
			{
				char buf[160];
				std::snprintf(buf, sizeof(buf), "Pathing: %s release check failed", pack.displayName);
				SetStatus(buf);
				/* Keep existing file if present. */
				return FileExistsNonEmpty(filePath);
			}
		}
		else
		{
			url = pack.directUrl ? pack.directUrl : "";
			const std::wstring urlW = Utf8ToWide(url.c_str());
			if (!HttpHeadStamp(urlW.c_str(), wantStamp))
			{
				/* No HEAD — refresh only when missing or forced. */
				wantStamp = "direct";
			}
		}

		std::string haveStamp;
		const bool haveVer = ReadStamp(verPath, haveStamp);
		const bool haveFile = FileExistsNonEmpty(filePath);
		if (!force && haveFile && haveVer && haveStamp == wantStamp)
			return true;

		{
			char buf[160];
			std::snprintf(buf, sizeof(buf), "Pathing: downloading %s…", pack.displayName);
			SetStatus(buf);
		}

		DeleteFileW(tmpPath.c_str());
		const std::wstring urlW = Utf8ToWide(url.c_str());
		if (!DownloadToFile(urlW.c_str(), tmpPath) || !FileExistsNonEmpty(tmpPath))
		{
			char buf[160];
			std::snprintf(buf, sizeof(buf), "Pathing: %s download failed", pack.displayName);
			SetStatus(buf);
			DeleteFileW(tmpPath.c_str());
			return haveFile; /* keep previous */
		}

		DeleteFileW(filePath.c_str());
		if (!MoveFileW(tmpPath.c_str(), filePath.c_str()))
		{
			/* Fallback copy+delete */
			if (!CopyFileW(tmpPath.c_str(), filePath.c_str(), FALSE))
			{
				DeleteFileW(tmpPath.c_str());
				return false;
			}
			DeleteFileW(tmpPath.c_str());
		}
		WriteStamp(verPath, wantStamp);
		{
			char buf[160];
			std::snprintf(buf, sizeof(buf), "Pathing: %s ready (%s)", pack.displayName, wantStamp.c_str());
			SetStatus(buf);
		}
		return true;
	}
}

void PathingPacks::RequestForceUpdate()
{
	gForceUpdate.store(true, std::memory_order_release);
}

bool PathingPacks::IsUpdating()
{
	return gUpdating.load(std::memory_order_acquire);
}

void PathingPacks::GetStatus(char* out, size_t outLen)
{
	if (!out || outLen == 0)
		return;
	std::lock_guard<std::mutex> lock(gStatusMu);
	std::snprintf(out, outLen, "%s", gStatus);
}

bool PathingPacks::EnsureCurated(const wchar_t* pathingDirWide)
{
	if (!pathingDirWide || !pathingDirWide[0])
		return false;
	gUpdating.store(true, std::memory_order_release);
	const bool force = gForceUpdate.exchange(false, std::memory_order_acq_rel);
	SetStatus(force ? "Pathing: checking curated packs (force)…" : "Pathing: checking curated packs…");

	CreateDirectoryW(pathingDirWide, nullptr);
	const std::wstring dir(pathingDirWide);
	bool anyOk = false;
	for (const CuratedPack& pack : kPacks)
	{
		if (EnsureOne(dir, pack, force))
			anyOk = true;
	}
	if (anyOk)
		SetStatus("Pathing: curated packs ready (user .taco files kept)");
	gUpdating.store(false, std::memory_order_release);
	return anyOk;
}
