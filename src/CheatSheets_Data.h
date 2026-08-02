#pragma once

#include "CheatSheets.h"

#include <cstddef>
#include <string>

/* Offline cheat-sheet HTML builders (raw string blobs). */
namespace CheatSheetsData
{
	struct PageSpec
	{
		CheatSheets::Sheet meta;
		std::string (*build)();
	};

	const PageSpec* Pages(size_t* outCount);
}
