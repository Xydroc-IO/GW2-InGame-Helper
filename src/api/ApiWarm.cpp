#include "ApiWarm.h"

#include "Globals.h"
#include "Gw2Catalog.h"
#include "VaultPad.h"
#include "WalletPad.h"

#include <cstdio>
#include <cstring>

#include <windows.h>

void ApiWarm::Tick()
{
	static char sKey[128]{};
	static bool sHelperSeen = false;
	static DWORD sDebounceUntil = 0;
	static DWORD sRetryUntil = 0;
	static DWORD sNextTry = 0;

	const DWORD now = GetTickCount();
	Gw2Catalog::Tick();

	if (std::strncmp(sKey, G::Gw2ApiKey, sizeof(sKey)) != 0)
	{
		std::snprintf(sKey, sizeof(sKey), "%s", G::Gw2ApiKey);
		if (sKey[0])
		{
			/* Wait for paste/typing to settle before the first GETs. */
			sDebounceUntil = now + 800u;
			sRetryUntil = now + 14000u;
			sNextTry = sDebounceUntil;
		}
	}

	if (G::ShowWiki)
	{
		if (!sHelperSeen && G::Gw2ApiKey[0])
		{
			sDebounceUntil = now;
			sRetryUntil = now + 14000u;
			sNextTry = now;
		}
		sHelperSeen = true;
	}
	else
		sHelperSeen = false;

	if (!G::Gw2ApiKey[0] || sRetryUntil == 0)
		return;
	if (now < sDebounceUntil || now < sNextTry || now > sRetryUntil)
		return;

	sNextTry = now + 400u;
	WalletPad::RefreshData(false);
	VaultPad::RefreshData();
}
