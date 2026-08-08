#include "WatchLinuxInternal.h"

#include "AddonPaths.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <ws2tcpip.h>

using namespace WatchLinuxDetail;

namespace WatchLinuxDetail
{
	bool EnsureWsa()
	{
		if (gWsa)
			return true;
		WSADATA wsa{};
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
			return false;
		gWsa = true;
		return true;
	}

	bool SendAll(SOCKET s, const void* data, int n)
	{
		const char* p = static_cast<const char*>(data);
		int off = 0;
		while (off < n)
		{
			const int w = ::send(s, p + off, n - off, 0);
			if (w <= 0)
				return false;
			off += w;
		}
		return true;
	}

	bool RecvAll(SOCKET s, void* data, int n)
	{
		char* p = static_cast<char*>(data);
		int off = 0;
		while (off < n)
		{
			const int r = ::recv(s, p + off, n - off, 0);
			if (r <= 0)
				return false;
			off += r;
		}
		return true;
	}

	bool SendCmd(SOCKET s, uint32_t type, const void* payload, uint32_t nbytes)
	{
		WatchProto::Header h{ WatchProto::kMagic, type, nbytes };
		if (!SendAll(s, &h, sizeof(h)))
			return false;
		if (nbytes && payload)
			return SendAll(s, payload, static_cast<int>(nbytes));
		return true;
	}

	bool RecvMsg(SOCKET s, uint32_t& type, std::vector<uint8_t>& body)
	{
		WatchProto::Header h{};
		if (!RecvAll(s, &h, sizeof(h)))
			return false;
		if (h.magic != WatchProto::kMagic)
			return false;
		type = h.type;
		body.resize(h.nbytes);
		if (h.nbytes && !RecvAll(s, body.data(), static_cast<int>(h.nbytes)))
			return false;
		return true;
	}

	std::string WinToUnixPath(const std::wstring& win)
	{
		/* Wine/Proton maps the Linux tree as a drive (usually Z:). Accept any
		   "X:\home\…" style path, not only Z:. */
		if (win.size() >= 2 && ((win[0] >= L'A' && win[0] <= L'Z') ||
				(win[0] >= L'a' && win[0] <= L'z')) && win[1] == L':')
		{
			std::string u;
			for (size_t i = 2; i < win.size(); ++i)
			{
				const wchar_t c = win[i];
				if (c == L'\\')
					u.push_back('/');
				else if (c < 128)
					u.push_back(static_cast<char>(c));
				else
					return {}; /* non-ASCII — need winepath; fail closed */
			}
			if (u.empty() || u[0] != '/')
				u.insert(u.begin(), '/');
			return u;
		}
		return {};
	}

	bool HostUnixShEx(const char* shScriptAndArgs, bool wait)
	{
		if (!shScriptAndArgs || !shScriptAndArgs[0])
			return false;
		char cmd[1900]{};
		std::snprintf(cmd, sizeof(cmd),
			wait ? "cmd.exe /c start /wait /unix /bin/sh -c %s"
			     : "cmd.exe /c start /unix /bin/sh -c %s",
			shScriptAndArgs);
		STARTUPINFOA si{};
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi{};
		if (!CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
				nullptr, nullptr, &si, &pi))
			return false;
		DWORD code = 1;
		if (wait)
		{
			WaitForSingleObject(pi.hProcess, 10000);
			GetExitCodeProcess(pi.hProcess, &code);
		}
		else
			code = 0;
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
		return wait ? (code == 0) : true;
	}

	bool HostUnixSh(const char* shScriptAndArgs)
	{
		return HostUnixShEx(shScriptAndArgs, /*wait=*/false);
	}

	void ChmodWatchdUnix(const std::string& unixBin)
	{
		if (unixBin.empty())
			return;
		/* $1 keeps spaces in "Guild Wars 2" out of the -c quote nest. */
		char arg[1400]{};
		std::snprintf(arg, sizeof(arg),
			"\"chmod +x -- \\\"$1\\\"\" _ \"%s\"", unixBin.c_str());
		HostUnixShEx(arg, /*wait=*/true);
	}

	extern "C" {
		extern const unsigned char _binary_build_watchd_blob_start[];
		extern const unsigned char _binary_build_watchd_blob_end[];
	}

	bool WriteAllBytesW(const std::wstring& path, const void* data, size_t size)
	{
		HANDLE out = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (out == INVALID_HANDLE_VALUE)
			return false;
		DWORD written = 0;
		const BOOL ok = WriteFile(out, data, static_cast<DWORD>(size), &written, nullptr);
		CloseHandle(out);
		return ok && written == size;
	}

	bool ExtractWatchd()
	{
		const unsigned char* begin = _binary_build_watchd_blob_start;
		const unsigned char* end = _binary_build_watchd_blob_end;
		if (end <= begin)
			return false;
		const size_t size = static_cast<size_t>(end - begin);
		const std::wstring path = AddonPaths::DataDir() + L"\\gw2igh-watchd";
		static constexpr const char* kStamp = "w9";
		const std::wstring verPath = path + L".ver";

		bool stampOk = false;
		HANDLE verIn = CreateFileW(verPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (verIn != INVALID_HANDLE_VALUE)
		{
			char buf[32]{};
			DWORD got = 0;
			if (ReadFile(verIn, buf, sizeof(buf) - 1, &got, nullptr) && got > 0)
				stampOk = (std::strncmp(buf, kStamp, std::strlen(kStamp)) == 0);
			CloseHandle(verIn);
		}

		HANDLE existing = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (existing != INVALID_HANDLE_VALUE)
		{
			LARGE_INTEGER li{};
			const bool same = stampOk && GetFileSizeEx(existing, &li) &&
				static_cast<size_t>(li.QuadPart) == size;
			CloseHandle(existing);
			if (same)
			{
				/* Always refresh space-free /tmp copy + chmod — Wine extract has no +x. */
				WriteAllBytesW(L"\\\\?\\unix\\/tmp/gw2igh-watchd", begin, size);
				ChmodWatchdUnix("/tmp/gw2igh-watchd");
				const std::string addonUnix = WinToUnixPath(AddonPaths::DataDir());
				if (!addonUnix.empty())
					ChmodWatchdUnix(addonUnix + "/gw2igh-watchd");
				return true;
			}
		}

		if (!WriteAllBytesW(path, begin, size))
			return false;
		/* Space-free spawn path — "Guild Wars 2" breaks fragile start quoting. */
		WriteAllBytesW(L"\\\\?\\unix\\/tmp/gw2igh-watchd", begin, size);
		ChmodWatchdUnix("/tmp/gw2igh-watchd");
		{
			const std::string addonUnix = WinToUnixPath(AddonPaths::DataDir());
			if (!addonUnix.empty())
				ChmodWatchdUnix(addonUnix + "/gw2igh-watchd");
		}

		HANDLE verOut = CreateFileW(verPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (verOut != INVALID_HANDLE_VALUE)
		{
			DWORD vw = 0;
			WriteFile(verOut, kStamp, static_cast<DWORD>(std::strlen(kStamp)), &vw, nullptr);
			CloseHandle(verOut);
		}
		return true;
	}

	void KillOldDaemon()
	{
		const std::wstring wpid = AddonPaths::DataDir() + L"\\gw2igh-watchd.pid";
		HANDLE f = CreateFileW(wpid.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (f == INVALID_HANDLE_VALUE)
			return;
		char buf[64]{};
		DWORD got = 0;
		ReadFile(f, buf, sizeof(buf) - 1, &got, nullptr);
		CloseHandle(f);
		const int pid = std::atoi(buf);
		if (pid <= 1)
			return;
		char cmd[512]{};
		std::snprintf(cmd, sizeof(cmd),
			"cmd.exe /c start /unix /bin/sh -c \"kill %d 2>/dev/null; "
			"fuser -k %u/tcp 2>/dev/null; true\"",
			pid, static_cast<unsigned>(WatchProto::kPort));
		STARTUPINFOA si{};
		si.cb = sizeof(si);
		PROCESS_INFORMATION pi{};
		if (CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
				nullptr, nullptr, &si, &pi))
		{
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			Sleep(200);
		}
	}

	bool TryConnectSock(SOCKET& out)
	{
		out = INVALID_SOCKET;
		if (!EnsureWsa())
			return false;
		SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (s == INVALID_SOCKET)
			return false;
		BOOL nd = TRUE;
		setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&nd), sizeof(nd));

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(WatchProto::kPort);
		inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
		u_long nb = 1;
		ioctlsocket(s, FIONBIO, &nb);
		::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
		fd_set wset;
		FD_ZERO(&wset);
		FD_SET(s, &wset);
		timeval tv{ 0, 200000 }; /* 200ms — workers poll; keep game thread free */
		const int sel = select(0, nullptr, &wset, nullptr, &tv);
		nb = 0;
		ioctlsocket(s, FIONBIO, &nb);
		if (sel <= 0)
		{
			closesocket(s);
			return false;
		}
		gProtoVer = 0;
		for (int i = 0; i < 6; ++i)
		{
			u_long avail = 0;
			ioctlsocket(s, FIONREAD, &avail);
			if (avail < sizeof(WatchProto::Header))
				break;
			uint32_t type = 0;
			std::vector<uint8_t> body;
			if (!RecvMsg(s, type, body))
			{
				closesocket(s);
				return false;
			}
			if (type == WatchProto::MsgHello && body.size() >= 4)
				std::memcpy(&gProtoVer, body.data(), 4);
			else if (type != WatchProto::MsgStatus)
				break;
		}
		if (gProtoVer > 0 && gProtoVer < WatchProto::kVersion)
		{
			closesocket(s);
			return false;
		}
		out = s;
		return true;
	}

	bool SpawnDaemon()
	{
		ExtractWatchd();
		EnsureShmMapped();
		KillOldDaemon();

		const std::wstring data = AddonPaths::DataDir();
		std::string unixData = WinToUnixPath(data);
		if (unixData.empty())
			return false;

		/* Prefer /tmp binary (no spaces). Fall back to addon-dir extract. */
		const std::string binTmp = "/tmp/gw2igh-watchd";
		const std::string binAddon = unixData + "/gw2igh-watchd";
		const std::string* bins[] = { &binTmp, &binAddon };

		for (const std::string* bin : bins)
		{
			ChmodWatchdUnix(*bin);
			/* Async start — do not /wait (daemon stays up). $0/$1 keep spaces safe. */
			char arg[1600]{};
			std::snprintf(arg, sizeof(arg),
				"\"chmod +x -- \\\"$0\\\"; exec \\\"$0\\\" \\\"$1\\\"\" \"%s\" \"%s\"",
				bin->c_str(), unixData.c_str());
			if (!HostUnixShEx(arg, /*wait=*/false))
				continue;
			for (int attempt = 0; attempt < 12; ++attempt)
			{
				Sleep(100);
				SOCKET s = INVALID_SOCKET;
				if (TryConnectSock(s))
				{
					closesocket(s);
					return true;
				}
			}
		}
		return false;
	}

	bool ConnectShared()
	{
		EnsureCs();
		EnterCriticalSection(&gCs);
		if (gSock != INVALID_SOCKET)
		{
			LeaveCriticalSection(&gCs);
			return true;
		}
		LeaveCriticalSection(&gCs);

		EnsureShmMapped();
		SOCKET s = INVALID_SOCKET;
		if (!TryConnectSock(s))
		{
			if (!SpawnDaemon() || !TryConnectSock(s))
				return false;
		}

		EnterCriticalSection(&gCs);
		if (gSock != INVALID_SOCKET)
		{
			closesocket(s);
			LeaveCriticalSection(&gCs);
			return true;
		}
		gSock = s;
		LeaveCriticalSection(&gCs);
		return true;
	}
}
