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
	std::string gStatus = "Hover an item → Ctrl+Shift+I";
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

	bool SetClipboardUtf8(const std::string& text)
	{
		if (!OpenClipboard(nullptr))
			return false;
		EmptyClipboard();
		if (text.empty())
		{
			CloseClipboard();
			return true;
		}
		const int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
		if (wlen <= 0)
		{
			CloseClipboard();
			return false;
		}
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(wlen) * sizeof(wchar_t));
		if (!mem)
		{
			CloseClipboard();
			return false;
		}
		wchar_t* dst = static_cast<wchar_t*>(GlobalLock(mem));
		MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, dst, wlen);
		GlobalUnlock(mem);
		SetClipboardData(CF_UNICODETEXT, mem);
		CloseClipboard();
		return true;
	}

	bool ClearClipboard()
	{
		if (!OpenClipboard(nullptr))
			return false;
		EmptyClipboard();
		CloseClipboard();
		return true;
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
				CRYPT_STRING_BASE64, out->data(), &need, nullptr, nullptr) != 0;
	}

	/* Chat link format: https://wiki.guildwars2.com/wiki/Chat_link_format
	   Item (type 0x02): quantity u8, then 3-byte little-endian item id. */
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

	void SendKey(WORD vk, bool up)
	{
		INPUT in{};
		in.type = INPUT_KEYBOARD;
		in.ki.wVk = vk;
		/* Scan codes help Wine deliver keys more reliably. */
		in.ki.wScan = static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
		in.ki.dwFlags = KEYEVENTF_SCANCODE | (up ? KEYEVENTF_KEYUP : 0);
		if (vk == VK_RSHIFT || vk == VK_RCONTROL || vk == VK_RMENU)
			in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
		SendInput(1, &in, sizeof(INPUT));
	}

	void SendMouseBtn(DWORD flags)
	{
		INPUT in{};
		in.type = INPUT_MOUSE;
		in.mi.dwFlags = flags;
		SendInput(1, &in, sizeof(INPUT));
	}

	void Chord(WORD mod, WORD key)
	{
		SendKey(mod, false);
		Sleep(15);
		SendKey(key, false);
		Sleep(15);
		SendKey(key, true);
		Sleep(15);
		SendKey(mod, true);
	}

	bool AnyModifierDown()
	{
		return (GetAsyncKeyState(VK_CONTROL) & 0x8000) ||
			(GetAsyncKeyState(VK_SHIFT) & 0x8000) ||
			(GetAsyncKeyState(VK_MENU) & 0x8000) ||
			(GetAsyncKeyState('I') & 0x8000) ||
			(GetAsyncKeyState(VK_LCONTROL) & 0x8000) ||
			(GetAsyncKeyState(VK_RCONTROL) & 0x8000) ||
			(GetAsyncKeyState(VK_LSHIFT) & 0x8000) ||
			(GetAsyncKeyState(VK_RSHIFT) & 0x8000);
	}

	void WaitKeysReleased()
	{
		for (int i = 0; i < 80 && !gStop.load(); ++i)
		{
			if (!AnyModifierDown())
				return;
			Sleep(20);
		}
	}

	/* Match item_detail_popups: Shift+Click → Ctrl+A → Ctrl+X → Enter.
	   Must run with Ctrl/Shift from the hotkey fully released, and with the
	   wiki window NOT covering the hovered item. */
	bool CaptureHoveredChatCode(std::string* outCode)
	{
		const std::string previous = GetClipboardUtf8();
		ClearClipboard();
		Sleep(30);

		WaitKeysReleased();
		Sleep(40);

		/* Prefer left shift — some Wine layouts mishandle RSHIFT. */
		SendKey(VK_LSHIFT, false);
		Sleep(40);
		SendMouseBtn(MOUSEEVENTF_LEFTDOWN);
		Sleep(40);
		SendMouseBtn(MOUSEEVENTF_LEFTUP);
		Sleep(40);
		SendKey(VK_LSHIFT, true);
		Sleep(120);

		/* Chat should now contain the link; focus and cut it. */
		if (G::API && G::API->GameBinds_InvokeAsync)
			G::API->GameBinds_InvokeAsync(GB_UiChatFocus, 50);
		Sleep(100);

		Chord(VK_CONTROL, 'A');
		Sleep(50);
		Chord(VK_CONTROL, 'X'); /* cut — same as other Nexus wiki tools */
		Sleep(80);

		/* Enter closes/sends empty chat without triggering CloseOnEscape. */
		SendKey(VK_RETURN, false);
		Sleep(20);
		SendKey(VK_RETURN, true);
		Sleep(60);

		std::string clip;
		for (int i = 0; i < 10; ++i)
		{
			clip = GetClipboardUtf8();
			if (ExtractChatCode(clip, outCode))
				break;
			Sleep(30);
		}

		if (!previous.empty())
			SetClipboardUtf8(previous);
		else
			ClearClipboard();

		return ExtractChatCode(clip, outCode) || (!outCode->empty());
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
		BOOL sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
		if (sent)
			sent = WinHttpReceiveResponse(request, nullptr);
		if (sent)
		{
			DWORD avail = 0;
			while (WinHttpQueryDataAvailable(request, &avail) && avail > 0)
			{
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
			if (c == '"')
				break;
			if (c == '\\' && i < json.size())
			{
				const char n = json[i++];
				if (n == 'n') out.push_back('\n');
				else if (n == 't') out.push_back('\t');
				else if (n == 'u' && i + 3 < json.size())
					i += 4;
				else
					out.push_back(n);
			}
			else
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
			/* Still try wiki search with the raw chat code / text. */
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
			/* API failed (common on some Wine setups) — wiki still resolves chat codes. */
			SetStatus("Opening wiki via chat link…");
			WikiBrowser::Search(code);
		}
	}

	void RunLookup(bool allowMacro)
	{
		if (gBusy.exchange(true))
			return;
		std::thread([allowMacro]() {
			UI_ReleaseGameInput();

			/* Critical: do NOT open/cover the item with the wiki until after capture. */
			const bool wasOpen = G::ShowWiki;
			if (wasOpen)
			{
				G::ShowWiki = false;
				WikiBrowser::SetVisible(false);
				Sleep(80); /* let one frame hide the overlay over the inventory */
			}

			std::string code;
			if (ExtractChatCode(GetClipboardUtf8(), &code))
			{
				SetStatus("Using item link from clipboard…");
				OpenWikiForChatCode(code);
				gBusy.store(false);
				return;
			}

			if (!allowMacro)
			{
				SetStatus("Copy an item chat link [&…] then press Ctrl+Shift+I");
				if (wasOpen)
					G::ShowWiki = true;
				gBusy.store(false);
				return;
			}

			SetStatus("Capturing hovered item…");
			Log("ItemLookup: starting hover capture");

			bool ok = false;
			for (int attempt = 0; attempt < 2 && !ok; ++attempt)
			{
				ok = CaptureHoveredChatCode(&code);
				if (!ok)
					Sleep(80);
			}

			if (!ok)
			{
				SetStatus("No item link — hover item, release keys, retry");
				Log("ItemLookup: capture failed");
				if (wasOpen)
					G::ShowWiki = true;
				gBusy.store(false);
				return;
			}

			char buf[160];
			std::snprintf(buf, sizeof(buf), "Captured %s", code.c_str());
			Log(buf);
			OpenWikiForChatCode(code);
			gBusy.store(false);
		}).detach();
	}
}

void ItemLookup::Init()
{
	gStop.store(false);
	SetStatus("Hover an item → Ctrl+Shift+I");
}

void ItemLookup::Shutdown()
{
	gStop.store(true);
	gBusy.store(false);
}

void ItemLookup::LookupHoveredItem()
{
	RunLookup(true);
}

void ItemLookup::LookupClipboard()
{
	RunLookup(false);
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
