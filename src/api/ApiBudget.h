#pragma once

/* Process-wide cap on concurrent Gw2Http calls to api.guildwars2.com.
   Prevents Crafting + Wallet + Vault + Instances from stampeding the rate limit
   and hitching the client. Acquire blocks the worker thread (never Present). */
namespace ApiBudget
{
	/* Default max in-flight GW2 API GETs across the whole DLL. */
	constexpr int kDefaultMaxConcurrent = 6;

	void SetMaxConcurrent(int n);

	/* Wait up to waitMs for a slot. Returns false if timed out (caller should abort/retry). */
	bool Acquire(int waitMs = 30000);

	void Release();

	int InFlight();
	int MaxConcurrent();

	struct Slot
	{
		bool held = false;
		explicit Slot(int waitMs = 30000) { held = Acquire(waitMs); }
		~Slot() { if (held) Release(); }
		Slot(const Slot&) = delete;
		Slot& operator=(const Slot&) = delete;
		explicit operator bool() const { return held; }
	};
}
