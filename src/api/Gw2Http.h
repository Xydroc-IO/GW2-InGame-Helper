#pragma once

#include <string>

/* Read-only WinHTTP GET for Live panels (api.guildwars2.com, news feed, wiki).
   Runs in the main helper DLL — no second addon. */
namespace Gw2Http
{
	struct Result
	{
		bool        ok = false;
		unsigned    status = 0;
		std::string body;
		std::string error;
	};

	/* Optional bearerToken: GW2 API key (never logged). Empty = unauthenticated.
	   BLOCKING — call only from a background worker, never the game/UI thread. */
	Result Get(const char* url, const char* bearerToken = nullptr,
		int timeoutMs = 4000);

	/* Convenience: GET https://api.guildwars2.com/v2/... */
	Result Api(const char* pathAndQuery, const char* bearerToken = nullptr,
		int timeoutMs = 4000);
}
