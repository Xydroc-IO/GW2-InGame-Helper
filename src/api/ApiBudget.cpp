#include "ApiBudget.h"

#include <atomic>

#include <windows.h>

namespace
{
	std::atomic<int> gMax{ApiBudget::kDefaultMaxConcurrent};
	std::atomic<int> gInFlight{0};
}

void ApiBudget::SetMaxConcurrent(int n)
{
	if (n < 1) n = 1;
	if (n > 32) n = 32;
	gMax.store(n);
}

int ApiBudget::MaxConcurrent() { return gMax.load(); }
int ApiBudget::InFlight() { return gInFlight.load(); }

bool ApiBudget::Acquire(int waitMs)
{
	if (waitMs < 0) waitMs = 0;
	const DWORD start = GetTickCount();
	for (;;)
	{
		int cur = gInFlight.load();
		const int maxN = gMax.load();
		if (cur < maxN && gInFlight.compare_exchange_weak(cur, cur + 1))
			return true;
		if (waitMs == 0)
			return false;
		if (GetTickCount() - start >= static_cast<DWORD>(waitMs))
			return false;
		Sleep(15);
	}
}

void ApiBudget::Release()
{
	int cur = gInFlight.fetch_sub(1);
	if (cur <= 0)
		gInFlight.store(0);
}
