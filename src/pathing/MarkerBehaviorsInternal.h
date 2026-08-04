#pragma once

#include "PathingTrails.h"
#include "MarkerBehaviors.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

/* Shared state / helpers for MarkerBehaviors.cpp + MarkerBehaviorsState.cpp. */
namespace MarkerBehaviorsDetail
{
	constexpr DWORD kSaveDebounceMs = 1500u;
	constexpr DWORD kTickMs = 100u;

	struct Activation
	{
		int64_t  unixTime = 0;
		uint32_t mapId = 0;
		uint32_t shardId = 0;
		uint32_t instanceId = 0;
		std::string character;
		int behavior = 0;
		float resetLength = 0.f;
	};

	extern std::mutex gMu;
	extern std::unordered_map<std::string, Activation> gStates;
	extern bool gDirty;
	extern DWORD gLastSave;
	extern DWORD gLastTick;
	extern std::atomic<bool> gInteractReq;
	extern uint32_t gLastMapId;

	extern MarkerBehaviors::NearbyUi gNearby;
	extern char gInfoPopup[2048];
	extern bool gShowInfo;
	extern char gToast[160];
	extern DWORD gToastUntil;

	extern std::vector<std::string> gPendingShow;
	extern std::vector<std::string> gPendingHide;
	extern std::string gPendingCopy;
	extern std::string gPendingCopyMsg;
	extern std::string gPendingInfo;

	std::wstring PathW();
	bool WriteUtf8File(const std::wstring& path, const std::string& data);
	std::string ReadUtf8File(const std::wstring& path);
	int64_t NowUnix();
	int64_t LastWeeklyResetUnix(int64_t now);
	int64_t LastDailyResetUnix(int64_t now);
	bool CopyUtf8Clipboard(const char* text);
	void Toast(const char* msg);
	std::vector<std::string> SplitCsv(const char* csv);
	bool HasRuntime(const PathingTrails::Marker& m);
	float Dist3(float ax, float ay, float az, float bx, float by, float bz);
	float InteractRange(const PathingTrails::Marker& m);
	bool StillHiddenLocked(const Activation& a, int behavior, float resetLength,
		uint32_t mapId, uint32_t shardId, uint32_t instanceId,
		const char* characterName, int64_t now);
	bool IsHiddenLocked(const PathingTrails::Marker& m,
		uint32_t mapId, uint32_t shardId, uint32_t instanceId,
		const char* characterName);
	void QueueShowHide(const PathingTrails::Marker& m);
	void ActivateLocked(const PathingTrails::Marker& m,
		uint32_t mapId, uint32_t shardId, uint32_t instanceId,
		const char* characterName, int behaviorOverride = -1);
	bool QueueTrigger(const PathingTrails::Marker& m,
		uint32_t mapId, uint32_t shardId, uint32_t instanceId,
		const char* characterName, int behaviorOverride = -1);
	void FlushPending();
}
