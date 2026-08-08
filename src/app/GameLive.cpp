#include "GameLive.h"

#include "Globals.h"

#include <windows.h>

bool GameLive::IsLive()
{
	if (!G::Mumble)
		return false;
	static uint32_t s_lastTick = 0;
	static double s_lastChange = 0.0;
	const uint32_t tick = G::Mumble->uiTick;
	const double now = static_cast<double>(GetTickCount64()) / 1000.0;
	if (tick != s_lastTick)
	{
		s_lastTick = tick;
		s_lastChange = now;
	}
	if (tick == 0)
		return false;
	return (now - s_lastChange) < 0.1;
}

namespace
{
	bool UiFlag(uint32_t bit)
	{
		if (!G::Mumble || G::Mumble->context_len < sizeof(MumbleContext))
			return false;
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		return (ctx->uiState & bit) != 0;
	}
}

bool GameLive::IsMapOpen()
{
	return UiFlag(static_cast<uint32_t>(UiStateBits::MapOpen));
}

bool GameLive::GameHasFocus()
{
	return UiFlag(static_cast<uint32_t>(UiStateBits::GameFocus));
}

bool GameLive::TextboxHasFocus()
{
	return UiFlag(static_cast<uint32_t>(UiStateBits::TextboxFocus));
}

bool GameLive::IsInCombat()
{
	return UiFlag(static_cast<uint32_t>(UiStateBits::InCombat));
}
