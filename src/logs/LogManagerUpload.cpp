#include "LogManagerUpload.h"
#include "LogManagerUploadInternal.h"

#include "Globals.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>
#include <winhttp.h>

namespace LogManagerDetail
{
/* ---------- dps.report upload / metadata ---------- */

std::string UrlEncode(const std::string& s)
{
	std::string o;
	o.reserve(s.size() * 3);
	static const char* hex = "0123456789ABCDEF";
	for (unsigned char c : s)
	{
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~')
			o.push_back(static_cast<char>(c));
		else
		{
			o.push_back('%');
			o.push_back(hex[c >> 4]);
			o.push_back(hex[c & 15]);
		}
	}
	return o;
}

/* dps.report accepts id or URL — query param works most reliably as the bare id. */
std::string PermalinkQueryValue(const std::string& permalink)
{
	std::string s = permalink;
	while (!s.empty() && (s.back() == ' ' || s.back() == '\n' || s.back() == '\r'))
		s.pop_back();
	const auto slash = s.find_last_of('/');
	if (slash != std::string::npos && slash + 1 < s.size())
	{
		std::string id = s.substr(slash + 1);
		const auto q = id.find_first_of("?#");
		if (q != std::string::npos)
			id.resize(q);
		if (!id.empty())
			return id;
	}
	return s;
}


void ApplyDpsReportMeta(LogEntry& e, const std::string& resp)
{
	if (resp.empty())
		return;

	std::string link;
	if (JsonStringAfterKey(resp.c_str(), "permalink", link) && !link.empty())
		e.dpsReportUrl = link;

	/* Prefer nested encounter object fields. */
	const char* enc = std::strstr(resp.c_str(), "\"encounter\"");
	std::string encSlice;
	if (enc)
	{
		const char* brace = std::strchr(enc, '{');
		if (brace)
		{
			const char* end = ObjectEnd(brace);
			if (end)
				encSlice.assign(brace, end);
		}
	}
	const char* src = !encSlice.empty() ? encSlice.c_str() : resp.c_str();

	std::string boss;
	if (JsonStringAfterKey(src, "boss", boss) && !boss.empty())
		e.encounter = boss;

	bool success = false;
	if (JsonBoolAfterKey(src, "success", success))
		e.result = success ? 1 : 0;

	bool isCm = false;
	bool isLcm = false;
	JsonBoolAfterKey(src, "isCm", isCm);
	if (!isCm)
		JsonBoolAfterKey(src, "isCM", isCm);
	JsonBoolAfterKey(src, "isLegendaryCm", isLcm);
	if (!isLcm)
		JsonBoolAfterKey(src, "isLCM", isLcm);
	if (isLcm)
		e.mode = "LCM";
	else if (isCm)
		e.mode = "CM";

	long long durSec = 0;
	if (JsonLongAfterKey(src, "duration", durSec) && durSec > 0)
		e.durationMs = durSec * 1000;

	long long comp = 0;
	if (JsonLongAfterKey(src, "compDps", comp) && comp > 0)
		e.compDps = static_cast<int>(comp);

	long long encTime = 0;
	if (JsonLongAfterKey(resp.c_str(), "encounterTime", encTime) && encTime > 0)
		e.encounterTime = static_cast<time_t>(encTime);

	/* Metadata has names only — never replace a squad that already has DPS/boons. */
	if (e.players.empty() || PlayersNeedCombatStats(e.players))
	{
		if (e.players.empty() || !PlayersHaveDps(e.players))
			ParseDpsReportPlayers(resp.c_str(), e.players);
	}
	if (!e.encounter.empty() || e.result >= 0 || e.durationMs > 0)
	{
		e.parseError.clear();
		if (e.state != ParseState::Uploaded)
			e.state = ParseState::Parsed;
	}
}

bool FetchEiJsonFromReport(const std::string& permalink, std::string& json, std::string& err)
{
	json.clear();
	if (permalink.empty())
	{
		err = "No permalink.";
		return false;
	}
	std::string path = "/getJson?permalink=";
	path += UrlEncode(PermalinkQueryValue(permalink));
	/* EI JSON can be large — allow a longer read. */
	const std::wstring host = Utf8ToWide("dps.report");
	const std::wstring pathW = Utf8ToWide(path.c_str());
	HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/Logs",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session)
	{
		err = "WinHTTP open failed.";
		return false;
	}
	WinHttpSetTimeouts(session, 20000, 20000, 20000, 180000);
	HINTERNET conn = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!conn)
	{
		WinHttpCloseHandle(session);
		err = "Connect failed.";
		return false;
	}
	HINTERNET req = WinHttpOpenRequest(conn, L"GET", pathW.c_str(), nullptr,
		WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (!req)
	{
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);
		err = "OpenRequest failed.";
		return false;
	}
	DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
	WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
	if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
		!WinHttpReceiveResponse(req, nullptr))
	{
		WinHttpCloseHandle(req);
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);
		err = "getJson request failed.";
		return false;
	}
	char chunk[8192];
	DWORD n = 0;
	while (WinHttpReadData(req, chunk, sizeof(chunk), &n) && n > 0)
	{
		json.append(chunk, n);
		if (json.size() > 60 * 1024 * 1024)
		{
			WinHttpCloseHandle(req);
			WinHttpCloseHandle(conn);
			WinHttpCloseHandle(session);
			err = "getJson too large.";
			json.clear();
			return false;
		}
	}
	WinHttpCloseHandle(req);
	WinHttpCloseHandle(conn);
	WinHttpCloseHandle(session);
	if (json.find("\"players\"") == std::string::npos && json.find("\"Players\"") == std::string::npos)
	{
		err = "getJson missing players.";
		return false;
	}
	return true;
}

bool HttpGetSimple(const char* hostA, const char* pathAndQuery, std::string& body, std::string& err)
{
	body.clear();
	err.clear();
	const std::wstring host = Utf8ToWide(hostA);
	const std::wstring path = Utf8ToWide(pathAndQuery);
	HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/Logs",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session)
	{
		err = "WinHTTP open failed.";
		return false;
	}
	WinHttpSetTimeouts(session, 15000, 15000, 15000, 30000);
	HINTERNET conn = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!conn)
	{
		WinHttpCloseHandle(session);
		err = "Connect failed.";
		return false;
	}
	HINTERNET req = WinHttpOpenRequest(conn, L"GET", path.c_str(), nullptr,
		WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (!req)
	{
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);
		err = "OpenRequest failed.";
		return false;
	}
	DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
	WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
	if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
		!WinHttpReceiveResponse(req, nullptr))
	{
		WinHttpCloseHandle(req);
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);
		err = "Request failed.";
		return false;
	}
	char chunk[4096];
	DWORD n = 0;
	while (WinHttpReadData(req, chunk, sizeof(chunk), &n) && n > 0)
		body.append(chunk, n);
	WinHttpCloseHandle(req);
	WinHttpCloseHandle(conn);
	WinHttpCloseHandle(session);
	return !body.empty();
}

bool FetchDpsReportMeta(const std::string& permalink, std::string& resp, std::string& err)
{
	resp.clear();
	if (permalink.empty())
	{
		err = "No permalink.";
		return false;
	}
	std::string path = "/getUploadMetadata?permalink=";
	path += UrlEncode(PermalinkQueryValue(permalink));
	if (!HttpGetSimple("dps.report", path.c_str(), resp, err))
		return false;
	if (resp.find("\"permalink\"") == std::string::npos &&
		resp.find("\"encounter\"") == std::string::npos)
	{
		err = "Metadata response unexpected.";
		return false;
	}
	return true;
}

bool UploadToDpsReport(const std::wstring& filePath, std::string& respOut, std::string& err)
{
	respOut.clear();
	err.clear();
	HANDLE h = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
	{
		err = "Cannot open log file.";
		return false;
	}
	LARGE_INTEGER sz{};
	GetFileSizeEx(h, &sz);
	if (sz.QuadPart <= 0 || sz.QuadPart > 120 * 1024 * 1024)
	{
		CloseHandle(h);
		err = "Log file size invalid.";
		return false;
	}
	std::string bodyFile(static_cast<size_t>(sz.QuadPart), '\0');
	DWORD got = 0;
	const BOOL rok = ReadFile(h, bodyFile.data(), static_cast<DWORD>(bodyFile.size()), &got, nullptr);
	CloseHandle(h);
	if (!rok)
	{
		err = "Read failed.";
		return false;
	}
	bodyFile.resize(got);

	std::wstring fileName = filePath;
	const auto slash = fileName.find_last_of(L"\\/");
	if (slash != std::wstring::npos)
		fileName = fileName.substr(slash + 1);
	const std::string fileNameUtf8 = WideToUtf8(fileName);

	char boundary[64];
	std::snprintf(boundary, sizeof(boundary), "----gw2igh%lx%lx",
		static_cast<unsigned long>(GetCurrentProcessId()),
		static_cast<unsigned long>(GetTickCount()));

	std::string form;
	form.reserve(bodyFile.size() + 512);
	form += "--";
	form += boundary;
	form += "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"";
	form += fileNameUtf8;
	form += "\"\r\nContent-Type: application/octet-stream\r\n\r\n";
	form += bodyFile;
	form += "\r\n--";
	form += boundary;
	form += "--\r\n";

	std::string url = "https://dps.report/uploadContent?json=1";
	if (G::DpsReportToken[0])
	{
		url += "&userToken=";
		url += G::DpsReportToken;
	}

	HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/Logs",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session)
	{
		err = "WinHTTP open failed.";
		return false;
	}
	WinHttpSetTimeouts(session, kUploadTimeoutMs, kUploadTimeoutMs, kUploadTimeoutMs, kUploadTimeoutMs);

	HINTERNET conn = WinHttpConnect(session, L"dps.report", INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!conn)
	{
		WinHttpCloseHandle(session);
		err = "Connect failed.";
		return false;
	}

	std::string pathQuery = "/uploadContent?json=1";
	if (G::DpsReportToken[0])
	{
		pathQuery += "&userToken=";
		pathQuery += G::DpsReportToken;
	}
	const std::wstring pathW = Utf8ToWide(pathQuery.c_str());

	HINTERNET req = WinHttpOpenRequest(conn, L"POST", pathW.c_str(), nullptr,
		WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (!req)
	{
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);
		err = "OpenRequest failed.";
		return false;
	}

	std::string ctype = "multipart/form-data; boundary=";
	ctype += boundary;
	const std::wstring ctypeW = Utf8ToWide(ctype.c_str());
	std::wstring headers = L"Content-Type: " + ctypeW + L"\r\n";

	BOOL ok = WinHttpSendRequest(req, headers.c_str(), static_cast<DWORD>(-1),
		form.data(), static_cast<DWORD>(form.size()), static_cast<DWORD>(form.size()), 0);
	if (!ok || !WinHttpReceiveResponse(req, nullptr))
	{
		WinHttpCloseHandle(req);
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);
		err = "Upload request failed.";
		return false;
	}

	std::string resp;
	char chunk[4096];
	DWORD n = 0;
	while (WinHttpReadData(req, chunk, sizeof(chunk), &n) && n > 0)
		resp.append(chunk, n);

	WinHttpCloseHandle(req);
	WinHttpCloseHandle(conn);
	WinHttpCloseHandle(session);

	std::string permalink;
	if (!JsonStringAfterKey(resp.c_str(), "permalink", permalink) || permalink.empty())
	{
		std::string apiErr;
		JsonStringAfterKey(resp.c_str(), "error", apiErr);
		err = apiErr.empty() ? "Upload failed (no permalink)." : apiErr;
		return false;
	}
	respOut = std::move(resp);
	(void)url;
	return true;
}

} // namespace LogManagerDetail
