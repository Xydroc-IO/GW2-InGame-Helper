#pragma once

#include "WatchCapture.h"

#include <cstdint>
#include <string>
#include <vector>

/* Wine/Proton → Linux gw2igh-watchd (OOP like CEF helper):
 * watchd captures; DLL only presents /dev/shm frames. */
namespace WatchLinux
{
	bool Available();
	/* Non-blocking: spawns watchd on a worker if needed. */
	void WarmAsync();
	bool EnsureDaemon(); /* sync — prefer WarmAsync / Start */
	void Disconnect();

	void RefreshWindowList(std::vector<WatchCapture::WindowEntry>& out, std::string& status);
	/* Non-blocking on Wine: queues portal start; status updates as it progresses. */
	bool Start(uint64_t id, std::string& status);
	void Stop(std::string& status);
	bool IsCapturing();
	bool IsStarting(); /* daemon/portal kickoff in flight */
	bool PumpRunning();
	void PollJoinStart(); /* non-blocking reap of legacy StartWorker after Stop */
	/* Soft Start: create pump only when cooldown is clear (never on click frame). */
	bool ConsumeNeedPump();
	void EnsurePumpNow();
	uint64_t TargetId();
	void GetStatus(std::string& out);

	/* CEF-style present: pin shm slot, return pixels; call EndPresent after GPU upload. */
	bool BeginPresent(const uint8_t*& bgra, uint32_t& w, uint32_t& h, uint32_t& stride,
		std::string& status);
	void EndPresent();
}
