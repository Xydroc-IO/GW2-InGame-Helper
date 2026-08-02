/* Host (Linux) smoke test — WikiIpc packed layout + magic.
   Catches accidental sizeof / constant drift without Wine or CEF. */

#include "WikiIpc.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

static int gFails = 0;

static void Expect(bool ok, const char* msg)
{
	if (!ok)
	{
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++gFails;
	}
}

int main()
{
	Expect(kWikiIpcMagic == 0x484C4935u, "kWikiIpcMagic must be HLI5");
	Expect(kWikiFrameMaxW == 1920u, "kWikiFrameMaxW");
	Expect(kWikiFrameMaxH == 1200u, "kWikiFrameMaxH");
	Expect(kWikiFrameBufferCount == 2u, "double-buffered frames");
	Expect(kWikiInputQueueSize == 256u, "input ring size");
	Expect(kWikiCmdQueueSize == 32u, "cmd ring size");
	Expect(kWikiMaxTabs == 8, "kWikiMaxTabs");

	/* Packed contract — keep in sync with helper + DLL. */
	Expect(sizeof(WikiInputEvent) == 28u, "WikiInputEvent sizeof (pack 1)");
	Expect(sizeof(WikiCmdEvent) == 4u + 4u + 1536u, "WikiCmdEvent sizeof");
	Expect(sizeof(WikiIpcState) > 10000u, "WikiIpcState should include rings + open_ext");
	Expect(offsetof(WikiIpcState, magic) == 0, "magic at offset 0");

	char ipc[96]{}, frame[96]{}, wake[96]{};
	WikiIpcFormatNames(1234, ipc, frame, wake, sizeof(ipc));
	Expect(std::strstr(ipc, "1234") != nullptr, "IPC name embeds PID");
	Expect(std::strstr(frame, "1234") != nullptr, "frame name embeds PID");
	Expect(std::strstr(wake, "1234") != nullptr, "wake name embeds PID");
	Expect(std::strncmp(ipc, "Local\\GW2InGameHelper_CEF_IPC_v5_", 32) == 0, "IPC name prefix");

	if (gFails)
	{
		std::fprintf(stderr, "test_wiki_ipc: %d failure(s)\n", gFails);
		return 1;
	}
	std::printf("test_wiki_ipc: OK (WikiIpcState=%zu bytes)\n", sizeof(WikiIpcState));
	return 0;
}
