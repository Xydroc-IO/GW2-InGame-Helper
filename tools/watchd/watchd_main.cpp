/* gw2igh-watchd — Linux portal/PipeWire capture for Wine Watch pad.
 * Captures into /dev/shm; TCP is control-only (CEF-helper style OOP).
 * Usage: gw2igh-watchd <unix-data-dir>
 */
#include "watchd_internal.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace WatchdDetail
{
	namespace
	{
		std::thread gCapThread;
		std::atomic<bool> gCapThreadLive{ false };

		bool SendAll(int fd, const void* data, size_t n)
		{
			const char* p = static_cast<const char*>(data);
			size_t off = 0;
			while (off < n)
			{
				const ssize_t w = ::send(fd, p + off, n - off, MSG_NOSIGNAL);
				if (w <= 0)
					return false;
				off += static_cast<size_t>(w);
			}
			return true;
		}

		bool RecvAll(int fd, void* data, size_t n)
		{
			char* p = static_cast<char*>(data);
			size_t off = 0;
			while (off < n)
			{
				const ssize_t r = ::recv(fd, p + off, n - off, 0);
				if (r <= 0)
					return false;
				off += static_cast<size_t>(r);
			}
			return true;
		}

		bool SendMsg(int fd, uint32_t type, const void* payload, uint32_t nbytes)
		{
			WatchProto::Header h{ WatchProto::kMagic, type, nbytes };
			if (!SendAll(fd, &h, sizeof(h)))
				return false;
			if (nbytes && payload)
				return SendAll(fd, payload, nbytes);
			return true;
		}

		bool SendStatus(int fd, const char* s)
		{
			return SendMsg(fd, WatchProto::MsgStatus, s, static_cast<uint32_t>(std::strlen(s)));
		}

		bool SendErr(int fd, const char* s)
		{
			return SendMsg(fd, WatchProto::MsgErr, s, static_cast<uint32_t>(std::strlen(s)));
		}

		void WritePid()
		{
			const std::string path = gDataDir + "/gw2igh-watchd.pid";
			FILE* f = std::fopen(path.c_str(), "w");
			if (!f)
				return;
			std::fprintf(f, "%d\n", static_cast<int>(::getpid()));
			std::fclose(f);
		}

		void StopCapture()
		{
			gWantCapture = false;
			if (gCapThreadLive.load() && gCapThread.joinable())
			{
				gCapThread.join();
				gCapThreadLive = false;
			}
			SetCapturing(false);
		}

		void StartCaptureAsync(int replyFd)
		{
			StopCapture();
			gWantCapture = true;
			gCapThreadLive = true;
			gCapThread = std::thread([replyFd]() {
				std::string err;
				SendStatus(replyFd, "Portal picker opening on the Linux desktop…");
				const bool ok = RunPortalCaptureLoop(err);
				if (!ok && !err.empty())
				{
					gLastErr = err;
					SendErr(replyFd, err.c_str());
				}
				else
					SendStatus(replyFd, "Capture ended.");
				gWantCapture = false;
				SetCapturing(false);
			});
		}

		void ServeClient(int fd)
		{
			int one = 1;
			setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
			EnsureShm();
			uint32_t ver = WatchProto::kVersion;
			SendMsg(fd, WatchProto::MsgHello, &ver, 4);
			SendStatus(fd, "gw2igh-watchd ready (portal/PipeWire → shm).");

			for (;;)
			{
				WatchProto::Header h{};
				if (!RecvAll(fd, &h, sizeof(h)))
					break;
				if (h.magic != WatchProto::kMagic)
					break;
				std::vector<uint8_t> body(h.nbytes);
				if (h.nbytes && !RecvAll(fd, body.data(), h.nbytes))
					break;

				switch (h.type)
				{
				case WatchProto::CmdList:
				{
					/* Portal owns window selection — empty list + status. */
					uint32_t count = 0;
					SendMsg(fd, WatchProto::MsgWindows, &count, 4);
					SendStatus(fd, "Use Start — desktop portal picks the window.");
					break;
				}
				case WatchProto::CmdStop:
					StopCapture();
					SendStatus(fd, "Stopped.");
					break;
				case WatchProto::CmdStart:
					if (gPortalBusy.load() || gWantCapture.load())
					{
						SendStatus(fd, "Capture already starting or running.");
						break;
					}
					StartCaptureAsync(fd);
					break;
				case WatchProto::CmdPing:
					SendMsg(fd, WatchProto::MsgPong, nullptr, 0);
					break;
				default:
					SendErr(fd, "Unknown command.");
					break;
				}
			}
			StopCapture();
		}
	}
}

using namespace WatchdDetail;

int main(int argc, char** argv)
{
	if (argc > 1 && argv[1] && argv[1][0])
		gDataDir = argv[1];

	if (!EnsureShm())
	{
		std::fprintf(stderr, "gw2igh-watchd: shm map failed\n");
		return 1;
	}
	WritePid();

	int gListenFd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (gListenFd < 0)
	{
		std::perror("socket");
		return 1;
	}
	int yes = 1;
	setsockopt(gListenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(WatchProto::kPort);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(gListenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
		listen(gListenFd, 2) < 0)
	{
		std::perror("bind/listen");
		return 1;
	}
	std::fprintf(stderr, "gw2igh-watchd v%u portal/PipeWire on 127.0.0.1:%u dir=%s max %ux%u\n",
		WatchProto::kVersion, WatchProto::kPort, gDataDir.c_str(),
		WatchProto::kMaxW, WatchProto::kMaxH);

	for (;;)
	{
		const int cfd = accept(gListenFd, nullptr, nullptr);
		if (cfd < 0)
			continue;
		ServeClient(cfd);
		::close(cfd);
	}
}
