#include "CssProxy.h"

#include "CssCompat.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>
#include <winhttp.h>

#include "include/capi/cef_callback_capi.h"
#include "include/capi/cef_response_capi.h"
#include "include/internal/cef_string.h"

namespace
{
	using Fn_cef_string_from_utf8 = int (*)(const char*, size_t, cef_string_utf16_t*);
	using Fn_cef_string_clear = void (*)(cef_string_t*);

	Fn_cef_string_from_utf8 g_string_from_utf8 = nullptr;
	Fn_cef_string_clear g_string_clear = nullptr;

	void EnsureCefStringFns()
	{
		if (g_string_from_utf8)
			return;
		HMODULE cef = GetModuleHandleW(L"libcef.dll");
		if (!cef)
			return;
		g_string_from_utf8 = reinterpret_cast<Fn_cef_string_from_utf8>(
			GetProcAddress(cef, "cef_string_utf8_to_utf16"));
		g_string_clear = reinterpret_cast<Fn_cef_string_clear>(
			GetProcAddress(cef, "cef_string_utf16_clear"));
	}

	void MakeCefString(cef_string_t* out, const char* utf8)
	{
		EnsureCefStringFns();
		std::memset(out, 0, sizeof(*out));
		if (g_string_from_utf8 && utf8 && utf8[0])
			g_string_from_utf8(utf8, std::strlen(utf8), out);
	}

	void ClearCefString(cef_string_t* s)
	{
		EnsureCefStringFns();
		if (s && g_string_clear)
			g_string_clear(s);
	}

	std::string ToLower(std::string s)
	{
		for (char& c : s)
		{
			if (c >= 'A' && c <= 'Z')
				c = static_cast<char>(c - 'A' + 'a');
		}
		return s;
	}

	std::string UrlPath(const std::string& url)
	{
		const size_t scheme = url.find("://");
		size_t start = scheme == std::string::npos ? 0 : scheme + 3;
		const size_t slash = url.find('/', start);
		std::string path = slash == std::string::npos ? std::string() : url.substr(slash);
		const size_t q = path.find('?');
		if (q != std::string::npos)
			path.resize(q);
		return ToLower(path);
	}

	std::string UrlHost(const std::string& url)
	{
		const size_t scheme = url.find("://");
		size_t start = scheme == std::string::npos ? 0 : scheme + 3;
		size_t end = url.find('/', start);
		if (end == std::string::npos)
			end = url.find('?', start);
		if (end == std::string::npos)
			end = url.size();
		std::string host = url.substr(start, end - start);
		const size_t colon = host.find(':');
		if (colon != std::string::npos)
			host.resize(colon);
		return ToLower(host);
	}

	bool HostEndsWith(const std::string& host, const char* suffix)
	{
		const size_t n = std::strlen(suffix);
		return host.size() >= n && host.compare(host.size() - n, n, suffix) == 0;
	}

	/* Session memory cache — no disk. Avoids re-downloading app-*.css every navigation. */
	std::mutex gCssCacheMu;
	std::unordered_map<std::string, std::string> gCssCache;

	bool HttpGet(const std::string& urlUtf8, std::string* outBody, std::wstring* outErr)
	{
		outBody->clear();
		const int wideLen = MultiByteToWideChar(CP_UTF8, 0, urlUtf8.c_str(), -1, nullptr, 0);
		if (wideLen <= 1)
		{
			if (outErr) *outErr = L"bad url";
			return false;
		}
		std::wstring wurl(static_cast<size_t>(wideLen - 1), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, urlUtf8.c_str(), -1, wurl.data(), wideLen);

		URL_COMPONENTSW uc{};
		uc.dwStructSize = sizeof(uc);
		uc.dwSchemeLength = static_cast<DWORD>(-1);
		uc.dwHostNameLength = static_cast<DWORD>(-1);
		uc.dwUrlPathLength = static_cast<DWORD>(-1);
		uc.dwExtraInfoLength = static_cast<DWORD>(-1);
		if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc))
		{
			if (outErr) *outErr = L"crack url failed";
			return false;
		}

		std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
		std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
		if (uc.dwExtraInfoLength)
			path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

		const bool https = uc.nScheme == INTERNET_SCHEME_HTTPS;
		HINTERNET session = WinHttpOpen(L"GW2Helper/1.1",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session)
		{
			if (outErr) *outErr = L"WinHttpOpen failed";
			return false;
		}

		WinHttpSetTimeouts(session, 5000, 5000, 10000, 20000);

		HINTERNET conn = WinHttpConnect(session, host.c_str(), uc.nPort, 0);
		if (!conn)
		{
			WinHttpCloseHandle(session);
			if (outErr) *outErr = L"WinHttpConnect failed";
			return false;
		}

		DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
		HINTERNET req = WinHttpOpenRequest(conn, L"GET", path.c_str(), nullptr,
			WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
		if (!req)
		{
			WinHttpCloseHandle(conn);
			WinHttpCloseHandle(session);
			if (outErr) *outErr = L"WinHttpOpenRequest failed";
			return false;
		}

		/* Prefer identity so we always get plain CSS (CEF 103 filter path was unreliable). */
		WinHttpAddRequestHeaders(req,
			L"Accept: text/css,*/*;q=0.1\r\nAccept-Encoding: identity\r\n",
			static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);

		BOOL ok = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
		if (ok)
			ok = WinHttpReceiveResponse(req, nullptr);
		if (!ok)
		{
			WinHttpCloseHandle(req);
			WinHttpCloseHandle(conn);
			WinHttpCloseHandle(session);
			if (outErr) *outErr = L"HTTP request failed";
			return false;
		}

		DWORD status = 0;
		DWORD statusSize = sizeof(status);
		WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
		if (status < 200 || status >= 300)
		{
			WinHttpCloseHandle(req);
			WinHttpCloseHandle(conn);
			WinHttpCloseHandle(session);
			if (outErr) *outErr = L"HTTP status not OK";
			return false;
		}

		std::string body;
		for (;;)
		{
			DWORD avail = 0;
			if (!WinHttpQueryDataAvailable(req, &avail))
				break;
			if (avail == 0)
				break;
			std::vector<char> buf(avail);
			DWORD read = 0;
			if (!WinHttpReadData(req, buf.data(), avail, &read) || read == 0)
				break;
			body.append(buf.data(), read);
			if (body.size() > 8u * 1024u * 1024u)
				break;
		}

		WinHttpCloseHandle(req);
		WinHttpCloseHandle(conn);
		WinHttpCloseHandle(session);

		*outBody = std::move(body);
		return !outBody->empty();
	}

	struct CssProxy
	{
		cef_resource_handler_t handler{};
		std::atomic<int> refs{1};
		std::string url;
		std::string body;
		size_t offset = 0;
		bool ok = false;
	};

	void CEF_CALLBACK ProxyAddRef(cef_base_ref_counted_t* base)
	{
		reinterpret_cast<CssProxy*>(base)->refs.fetch_add(1, std::memory_order_relaxed);
	}

	int CEF_CALLBACK ProxyRelease(cef_base_ref_counted_t* base)
	{
		auto* self = reinterpret_cast<CssProxy*>(base);
		const int n = self->refs.fetch_sub(1, std::memory_order_acq_rel) - 1;
		if (n == 0)
		{
			delete self;
			return 0;
		}
		return 1;
	}

	int CEF_CALLBACK ProxyHasOneRef(cef_base_ref_counted_t* base)
	{
		return reinterpret_cast<CssProxy*>(base)->refs.load() == 1 ? 1 : 0;
	}

	int CEF_CALLBACK ProxyHasAtLeastOneRef(cef_base_ref_counted_t* base)
	{
		return reinterpret_cast<CssProxy*>(base)->refs.load() > 0 ? 1 : 0;
	}

	int CEF_CALLBACK ProxyOpen(cef_resource_handler_t* self, cef_request_t*,
		int* handle_request, cef_callback_t*)
	{
		auto* proxy = reinterpret_cast<CssProxy*>(self);
		*handle_request = 1;

		{
			std::lock_guard<std::mutex> lock(gCssCacheMu);
			const auto it = gCssCache.find(proxy->url);
			if (it != gCssCache.end())
			{
				proxy->body = it->second;
				proxy->ok = true;
				return 1;
			}
		}

		std::string raw;
		std::wstring err;
		if (!HttpGet(proxy->url, &raw, &err))
		{
			proxy->ok = false;
			proxy->body = "/* snowcrow css proxy fetch failed */\n";
			return 1;
		}

		proxy->body = DownlevelCss(raw);
		proxy->ok = true;

		{
			std::lock_guard<std::mutex> lock(gCssCacheMu);
			if (gCssCache.size() > 32)
				gCssCache.clear();
			gCssCache[proxy->url] = proxy->body;
		}
		return 1;
	}

	void CEF_CALLBACK ProxyGetHeaders(cef_resource_handler_t* self, cef_response_t* response,
		int64* response_length, cef_string_t*)
	{
		auto* proxy = reinterpret_cast<CssProxy*>(self);
		if (!response)
			return;
		if (response->set_status)
			response->set_status(response, proxy->ok ? 200 : 500);
		if (response->set_mime_type)
		{
			cef_string_t mime{};
			MakeCefString(&mime, "text/css; charset=utf-8");
			response->set_mime_type(response, &mime);
			ClearCefString(&mime);
		}
		if (response->set_header_by_name)
		{
			cef_string_t name{};
			cef_string_t val{};
			MakeCefString(&name, "Cache-Control");
			MakeCefString(&val, "no-store");
			response->set_header_by_name(response, &name, &val, 1);
			ClearCefString(&name);
			ClearCefString(&val);
		}
		*response_length = static_cast<int64>(proxy->body.size());
	}

	int CEF_CALLBACK ProxyRead(cef_resource_handler_t* self, void* data_out, int bytes_to_read,
		int* bytes_read, cef_resource_read_callback_t*)
	{
		auto* proxy = reinterpret_cast<CssProxy*>(self);
		if (!data_out || bytes_to_read <= 0)
		{
			*bytes_read = 0;
			return 0;
		}
		if (proxy->offset >= proxy->body.size())
		{
			*bytes_read = 0;
			return 0;
		}
		const size_t remain = proxy->body.size() - proxy->offset;
		const size_t n = remain < static_cast<size_t>(bytes_to_read)
			? remain
			: static_cast<size_t>(bytes_to_read);
		std::memcpy(data_out, proxy->body.data() + proxy->offset, n);
		proxy->offset += n;
		*bytes_read = static_cast<int>(n);
		return 1;
	}

	int CEF_CALLBACK ProxySkip(cef_resource_handler_t* self, int64 bytes_to_skip,
		int64* bytes_skipped, cef_resource_skip_callback_t*)
	{
		auto* proxy = reinterpret_cast<CssProxy*>(self);
		const int64 remain = static_cast<int64>(proxy->body.size() - proxy->offset);
		const int64 n = bytes_to_skip < remain ? bytes_to_skip : remain;
		proxy->offset += static_cast<size_t>(n);
		*bytes_skipped = n;
		return 1;
	}

	void CEF_CALLBACK ProxyCancel(cef_resource_handler_t*) {}
}

bool ShouldBlockUrl(const std::string& url)
{
	const std::string host = UrlHost(url);
	const std::string path = UrlPath(url);

	/* Keep Google Fonts — Snow Crows depends on them. */
	if (host == "fonts.googleapis.com" || host == "fonts.gstatic.com")
		return false;

	if (HostEndsWith(host, "nitropay.com") ||
		HostEndsWith(host, "cookieinformation.com") ||
		HostEndsWith(host, "googlesyndication.com") ||
		HostEndsWith(host, "doubleclick.net") ||
		HostEndsWith(host, "googletagmanager.com") ||
		HostEndsWith(host, "google-analytics.com") ||
		HostEndsWith(host, "facebook.net") ||
		HostEndsWith(host, "facebook.com") ||
		HostEndsWith(host, "hotjar.com") ||
		host == "vjs.zencdn.net")
		return true;

	if ((host == "www.google.com" || host == "google.com" || HostEndsWith(host, "gstatic.com")) &&
		path.find("recaptcha") != std::string::npos)
		return true;

	return false;
}

bool ShouldProxyCss(const std::string& url)
{
	const std::string path = UrlPath(url);
	if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".css") == 0)
		return true;
	/* Vite-built hashed assets without query sometimes still end in .css */
	return path.find(".css") != std::string::npos;
}

cef_resource_handler_t* CreateCssProxyHandler(const std::string& url)
{
	auto* proxy = new (std::nothrow) CssProxy();
	if (!proxy)
		return nullptr;
	proxy->url = url;
	proxy->handler.base.size = sizeof(cef_resource_handler_t);
	proxy->handler.base.add_ref = ProxyAddRef;
	proxy->handler.base.release = ProxyRelease;
	proxy->handler.base.has_one_ref = ProxyHasOneRef;
	proxy->handler.base.has_at_least_one_ref = ProxyHasAtLeastOneRef;
	proxy->handler.open = ProxyOpen;
	proxy->handler.process_request = nullptr;
	proxy->handler.get_response_headers = ProxyGetHeaders;
	proxy->handler.skip = ProxySkip;
	proxy->handler.read = ProxyRead;
	proxy->handler.read_response = nullptr;
	proxy->handler.cancel = ProxyCancel;
	return &proxy->handler;
}
