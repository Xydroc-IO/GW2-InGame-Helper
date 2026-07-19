#include "ItemLookup.h"

#include "Globals.h"
#include "Settings.h"
#include "Sites.h"
#include "UI.h"
#include "WikiBrowser.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>
#include <wincrypt.h>
#include <winhttp.h>
#include <cwchar>

namespace
{
	std::mutex gMutex;
	std::string gStatus = "Copy [&…] then Ctrl+Shift+U";
	std::atomic<bool> gBusy{false};
	std::atomic<bool> gStop{false};

	void SetStatus(const char* text)
	{
		std::lock_guard<std::mutex> lock(gMutex);
		gStatus = text ? text : "";
	}

	void Log(const char* msg)
	{
		if (G::API && G::API->Log)
			G::API->Log(LOGL_INFO, ADDON_NAME, msg);
	}

	std::string GetClipboardUtf8()
	{
		std::string out;
		if (!OpenClipboard(nullptr))
			return out;
		HANDLE h = GetClipboardData(CF_UNICODETEXT);
		if (h)
		{
			const wchar_t* w = static_cast<const wchar_t*>(GlobalLock(h));
			if (w)
			{
				int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
				out.assign(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
				if (n > 0)
					WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), n, nullptr, nullptr);
				GlobalUnlock(h);
			}
		}
		CloseClipboard();
		return out;
	}

	bool ExtractChatCode(const std::string& text, std::string* outCode)
	{
		const auto start = text.find("[&");
		if (start == std::string::npos)
			return false;
		const auto end = text.find(']', start);
		if (end == std::string::npos || end <= start + 2)
			return false;
		*outCode = text.substr(start, end - start + 1);
		return true;
	}

	bool Base64Decode(const std::string& b64, std::vector<uint8_t>* out)
	{
		DWORD need = 0;
		if (!CryptStringToBinaryA(b64.c_str(), static_cast<DWORD>(b64.size()),
				CRYPT_STRING_BASE64, nullptr, &need, nullptr, nullptr))
			return false;
		out->assign(need, 0);
		return CryptStringToBinaryA(b64.c_str(), static_cast<DWORD>(b64.size()),
			CRYPT_STRING_BASE64, out->data(), &need, nullptr, nullptr) == TRUE;
	}

	bool DecodeItemId(const std::string& chatCode, uint32_t* outId)
	{
		if (chatCode.size() < 5 || chatCode[0] != '[' || chatCode[1] != '&' ||
			chatCode.back() != ']')
			return false;
		const std::string b64 = chatCode.substr(2, chatCode.size() - 3);
		std::vector<uint8_t> bytes;
		if (!Base64Decode(b64, &bytes) || bytes.size() < 5)
			return false;
		if (bytes[0] != 0x02) /* only item links for now */
			return false;
		*outId = static_cast<uint32_t>(bytes[2]) |
			(static_cast<uint32_t>(bytes[3]) << 8) |
			(static_cast<uint32_t>(bytes[4]) << 16);
		return *outId != 0;
	}

	std::string HttpGetUtf8(const wchar_t* host, const std::wstring& path)
	{
		std::string body;
		HINTERNET session = WinHttpOpen(L"GW2-InGame-Helper/1.0",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!session)
			return body;
		WinHttpSetTimeouts(session, 3000, 3000, 5000, 5000);
		HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
		if (!connect)
		{
			WinHttpCloseHandle(session);
			return body;
		}
		HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(),
			nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
		if (!request)
		{
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
			return body;
		}
		if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
				WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
			WinHttpReceiveResponse(request, nullptr))
		{
			for (;;)
			{
				DWORD avail = 0;
				if (!WinHttpQueryDataAvailable(request, &avail) || avail == 0)
					break;
				std::string chunk(avail, '\0');
				DWORD read = 0;
				if (!WinHttpReadData(request, chunk.data(), avail, &read) || read == 0)
					break;
				chunk.resize(read);
				body += chunk;
			}
		}
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return body;
	}

	std::string JsonStringField(const std::string& json, const char* key)
	{
		const std::string needle = std::string("\"") + key + "\":\"";
		const auto pos = json.find(needle);
		if (pos == std::string::npos)
			return {};
		size_t i = pos + needle.size();
		std::string out;
		while (i < json.size())
		{
			const char c = json[i++];
			if (c == '\\' && i < json.size())
			{
				out.push_back(json[i++]);
				continue;
			}
			if (c == '"')
				break;
			out.push_back(c);
		}
		return out;
	}

	std::string FetchItemName(uint32_t itemId)
	{
		wchar_t path[96];
		std::swprintf(path, 96, L"/v2/items/%u?lang=en", itemId);
		const std::string json = HttpGetUtf8(L"api.guildwars2.com", path);
		return JsonStringField(json, "name");
	}

	void EnsureWikiSite()
	{
		if (Sites::SetActiveById("wiki"))
		{
			std::snprintf(G::ActiveSiteId, sizeof(G::ActiveSiteId), "%s", Sites::ActiveId());
			Settings::SetDirty();
		}
	}

	void OpenWikiForChatCode(const std::string& code)
	{
		EnsureWikiSite();

		uint32_t id = 0;
		if (!DecodeItemId(code, &id))
		{
			SetStatus("Opening wiki search…");
			G::ShowWiki = true;
			Settings::SetDirty();
			WikiBrowser::Search(code);
			return;
		}

		char buf[128];
		std::snprintf(buf, sizeof(buf), "Looking up item %u…", id);
		SetStatus(buf);
		Log(buf);

		const std::string name = FetchItemName(id);
		G::ShowWiki = true;
		Settings::SetDirty();

		if (!name.empty())
		{
			std::snprintf(buf, sizeof(buf), "Wiki: %s", name.c_str());
			SetStatus(buf);
			std::snprintf(G::LastQuery, sizeof(G::LastQuery), "%s", name.c_str());
			WikiBrowser::Search(name);
		}
		else
		{
			SetStatus("Opening wiki via chat link…");
			WikiBrowser::Search(code);
		}
	}

	/*
	 * Clipboard-only lookup. We deliberately do NOT SendInput Shift+Click / open chat —
	 * that path was mounting the player and posting to party chat under Wine/Proton.
	 */
	void RunLookup()
	{
		if (gBusy.exchange(true))
			return;
		std::thread([]() {
			UI_ReleaseGameInput();

			std::string code;
			if (!ExtractChatCode(GetClipboardUtf8(), &code))
			{
				SetStatus("Copy an item [&…] link first (Shift+Click item → Ctrl+C), then Ctrl+Shift+U");
				Log("ItemLookup: no [&…] on clipboard");
				gBusy.store(false);
				return;
			}

			SetStatus("Using item link from clipboard…");
			OpenWikiForChatCode(code);
			gBusy.store(false);
		}).detach();
	}
}

void ItemLookup::Init()
{
	gStop.store(false);
	SetStatus("Copy [&…] then Ctrl+Shift+U");
}

void ItemLookup::Shutdown()
{
	gStop.store(true);
	gBusy.store(false);
}

void ItemLookup::LookupHoveredItem()
{
	RunLookup();
}

void ItemLookup::LookupClipboard()
{
	RunLookup();
}

bool ItemLookup::IsBusy()
{
	return gBusy.load();
}

std::string ItemLookup::Status()
{
	std::lock_guard<std::mutex> lock(gMutex);
	return gStatus;
}
