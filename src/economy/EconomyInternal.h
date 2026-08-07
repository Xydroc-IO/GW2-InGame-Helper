#pragma once
#include "EconomyShared.h"

#include <mutex>

namespace EconomyDetail
{
	constexpr float kPadW = 440.f;
	constexpr float kPadH = 480.f;

	extern std::mutex gMu;
	const char* FallbackName(int id);
}
