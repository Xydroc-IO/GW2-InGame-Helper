#pragma once

#include "LogManagerShared.h"

#include <string>

/* Cross-TU helpers for LogManagerUi*.cpp detail/tabs. */
namespace LogManagerDetail
{
	const LogEntry* SelectedDrawEntry();
	std::string GuildLabelFor(const PlayerInfo& p);
	void KickKillProofForSelected(const LogEntry* sel, bool force);
}
