#pragma once

/* Prefetch account API (stash / vault) after helper open or API key save
   so pads paint from cache instead of waiting on ArenaNet. */
namespace ApiWarm
{
	void Tick();
}
