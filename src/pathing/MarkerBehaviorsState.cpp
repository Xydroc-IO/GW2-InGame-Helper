#include "MarkerBehaviors.h"
#include "MarkerBehaviorsInternal.h"

#include "AddonPaths.h"
#include "PathingTrails.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

namespace MarkerBehaviorsDetail
{
	std::mutex gMu;
	std::unordered_map<std::string, Activation> gStates;
	bool gDirty = false;
	DWORD gLastSave = 0;
	DWORD gLastTick = 0;
	std::atomic<bool> gInteractReq{false};
	uint32_t gLastMapId = 0;

	MarkerBehaviors::NearbyUi gNearby{};
	char gInfoPopup[2048]{};
	bool gShowInfo = false;
	char gToast[160]{};
	DWORD gToastUntil = 0;

	std::vector<std::string> gPendingShow;
	std::vector<std::string> gPendingHide;
	std::string gPendingCopy;
	std::string gPendingCopyMsg;
	std::string gPendingInfo;

std::wstring PathW()
{
	return AddonPaths::DataDir() + L"\\marker_behaviors.txt";
}

bool WriteUtf8File(const std::wstring& path, const std::string& data)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	DWORD written = 0;
	const BOOL ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
	CloseHandle(h);
	return ok && written == data.size();
}

std::string ReadUtf8File(const std::wstring& path)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return {};
	LARGE_INTEGER sz{};
	if (!GetFileSizeEx(h, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 8 * 1024 * 1024)
	{
		CloseHandle(h);
		return {};
	}
	std::string out(static_cast<size_t>(sz.QuadPart), '\0');
	DWORD read = 0;
	const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr);
	CloseHandle(h);
	if (!ok || read != out.size())
		return {};
	return out;
}

int64_t NowUnix()
{
	return static_cast<int64_t>(std::time(nullptr));
}

int64_t LastWeeklyResetUnix(int64_t now)
{
	std::tm utc{};
	const time_t t = static_cast<time_t>(now);
	gmtime_s(&utc, &t);
	const int daysFromMon = (utc.tm_wday + 6) % 7;
	const int64_t dayStart = now - (utc.tm_hour * 3600 + utc.tm_min * 60 + utc.tm_sec)
		- static_cast<int64_t>(daysFromMon) * 86400;
	int64_t thisWeek = dayStart + 7 * 3600 + 30 * 60;
	if (now < thisWeek)
		thisWeek -= 7 * 86400;
	return thisWeek;
}

int64_t LastDailyResetUnix(int64_t now)
{
	return (now / 86400) * 86400;
}

bool CopyUtf8Clipboard(const char* text)
{
	if (!text || !text[0])
		return false;
	if (!OpenClipboard(nullptr))
		return false;
	EmptyClipboard();
	const size_t n = std::strlen(text) + 1;
	HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, n);
	if (!mem)
	{
		CloseClipboard();
		return false;
	}
	void* ptr = GlobalLock(mem);
	if (!ptr)
	{
		GlobalFree(mem);
		CloseClipboard();
		return false;
	}
	std::memcpy(ptr, text, n);
	GlobalUnlock(mem);
	SetClipboardData(CF_TEXT, mem);
	CloseClipboard();
	return true;
}

void Toast(const char* msg)
{
	std::snprintf(gToast, sizeof(gToast), "%s", msg ? msg : "");
	gToastUntil = GetTickCount() + 3500u;
}

std::vector<std::string> SplitCsv(const char* csv)
{
	std::vector<std::string> out;
	if (!csv || !csv[0])
		return out;
	std::string cur;
	for (const char* p = csv; *p; ++p)
	{
		if (*p == ',' || *p == ';')
		{
			while (!cur.empty() && (cur.back() == ' ' || cur.back() == '\t'))
				cur.pop_back();
			size_t start = 0;
			while (start < cur.size() && (cur[start] == ' ' || cur[start] == '\t'))
				++start;
			if (start < cur.size())
				out.push_back(cur.substr(start));
			cur.clear();
		}
		else
			cur.push_back(*p);
	}
	while (!cur.empty() && (cur.back() == ' ' || cur.back() == '\t'))
		cur.pop_back();
	size_t start = 0;
	while (start < cur.size() && (cur[start] == ' ' || cur[start] == '\t'))
		++start;
	if (start < cur.size())
		out.push_back(cur.substr(start));
	return out;
}

bool HasRuntime(const PathingTrails::Marker& m)
{
	return m.guid[0] != 0 || m.behavior != 0 || m.hide[0] || m.show[0] ||
		m.autoTrigger || m.info[0] || m.copy[0] || m.tipName[0] || m.tipDescription[0];
}

float Dist3(float ax, float ay, float az, float bx, float by, float bz)
{
	const float dx = ax - bx;
	const float dy = ay - by;
	const float dz = az - bz;
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float InteractRange(const PathingTrails::Marker& m)
{
	if (m.triggerRange > 0.05f)
		return m.triggerRange;
	return 2.f;
}

bool StillHiddenLocked(const Activation& a, int behavior, float resetLength,
	uint32_t mapId, uint32_t shardId, uint32_t instanceId,
	const char* characterName, int64_t now)
{
	switch (behavior)
	{
	case 1:
		return a.mapId == mapId;
	case 2:
		return a.unixTime >= LastDailyResetUnix(now);
	case 3:
		return true;
	case 4:
	{
		const float secs = resetLength > 0.f ? resetLength : a.resetLength;
		return (now - a.unixTime) < static_cast<int64_t>(secs + 0.5f);
	}
	case 6:
		return a.mapId == mapId && a.shardId == shardId && a.instanceId == instanceId;
	case 7:
		if (characterName && characterName[0] && !a.character.empty() &&
			a.character != characterName)
			return false;
		return a.unixTime >= LastDailyResetUnix(now);
	case 101:
		return a.unixTime >= LastWeeklyResetUnix(now);
	default:
		return false;
	}
}

bool IsHiddenLocked(const PathingTrails::Marker& m,
	uint32_t mapId, uint32_t shardId, uint32_t instanceId,
	const char* characterName)
{
	if (m.behavior == 0 || !m.guid[0])
		return false;
	const auto it = gStates.find(m.guid);
	if (it == gStates.end())
		return m.invertBehavior;
	const int64_t now = NowUnix();
	const bool hidden = StillHiddenLocked(it->second, m.behavior, m.resetLength,
		mapId, shardId, instanceId, characterName, now);
	if (!hidden)
	{
		gStates.erase(it);
		gDirty = true;
		return m.invertBehavior;
	}
	return !m.invertBehavior;
}

void QueueShowHide(const PathingTrails::Marker& m)
{
	for (std::string& p : SplitCsv(m.hide))
		gPendingHide.push_back(std::move(p));
	for (std::string& p : SplitCsv(m.show))
		gPendingShow.push_back(std::move(p));
}

void ActivateLocked(const PathingTrails::Marker& m,
	uint32_t mapId, uint32_t shardId, uint32_t instanceId,
	const char* characterName, int behaviorOverride)
{
	const int behavior = behaviorOverride >= 0 ? behaviorOverride : m.behavior;
	if (behavior == 0 || !m.guid[0])
		return;
	Activation a;
	a.unixTime = NowUnix();
	a.mapId = mapId;
	a.shardId = shardId;
	a.instanceId = instanceId;
	a.behavior = behavior;
	a.resetLength = m.resetLength;
	if (characterName)
		a.character = characterName;
	gStates[m.guid] = std::move(a);
	gDirty = true;
}

/* Queue side-effects; does not call PathingTrails (avoid re-entrancy). */
bool QueueTrigger(const PathingTrails::Marker& m,
	uint32_t mapId, uint32_t shardId, uint32_t instanceId,
	const char* characterName, int behaviorOverride)
{
	const int behavior = behaviorOverride >= 0 ? behaviorOverride : m.behavior;
	{
		std::lock_guard<std::mutex> lock(gMu);
		PathingTrails::Marker check = m;
		check.behavior = behavior;
		if (behavior != 0 && IsHiddenLocked(check, mapId, shardId, instanceId, characterName) &&
			!m.invertBehavior)
			return false;
		ActivateLocked(m, mapId, shardId, instanceId, characterName, behavior);
	}
	QueueShowHide(m);
	if (m.copy[0] && gPendingCopy.empty())
	{
		gPendingCopy = m.copy;
		gPendingCopyMsg = m.copyMessage[0] ? m.copyMessage : "Copied to clipboard.";
	}
	if (m.info[0] && gPendingInfo.empty())
		gPendingInfo = m.info;
	return true;
}

void FlushPending()
{
	if (!gPendingShow.empty() || !gPendingHide.empty())
	{
		PathingTrails::ApplyCategoryShowHide(gPendingShow, gPendingHide);
		gPendingShow.clear();
		gPendingHide.clear();
	}
	if (!gPendingCopy.empty())
	{
		if (CopyUtf8Clipboard(gPendingCopy.c_str()))
			Toast(gPendingCopyMsg.c_str());
		else
			Toast("Clipboard copy failed.");
		gPendingCopy.clear();
		gPendingCopyMsg.clear();
	}
	if (!gPendingInfo.empty())
	{
		std::snprintf(gInfoPopup, sizeof(gInfoPopup), "%s", gPendingInfo.c_str());
		gShowInfo = true;
		gPendingInfo.clear();
	}
}
} // namespace MarkerBehaviorsDetail

using namespace MarkerBehaviorsDetail;

void MarkerBehaviors::Init()
{
	Load();
}

void MarkerBehaviors::Shutdown()
{
	Save(true);
}

void MarkerBehaviors::Load()
{
	std::lock_guard<std::mutex> lock(gMu);
	gStates.clear();
	const std::string raw = ReadUtf8File(PathW());
	if (raw.empty())
		return;
	size_t pos = 0;
	auto nextLine = [&](std::string& line) -> bool {
		if (pos >= raw.size())
			return false;
		size_t end = raw.find('\n', pos);
		if (end == std::string::npos)
			end = raw.size();
		line = raw.substr(pos, end - pos);
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		pos = end + 1;
		return true;
	};
	std::string line;
	if (!nextLine(line) || line.rfind("v1", 0) != 0)
		return;
	while (nextLine(line))
	{
		if (line.empty() || line[0] == '#')
			continue;
		std::vector<std::string> cols;
		size_t i = 0;
		while (i <= line.size())
		{
			size_t tab = line.find('\t', i);
			if (tab == std::string::npos)
			{
				cols.push_back(line.substr(i));
				break;
			}
			cols.push_back(line.substr(i, tab - i));
			i = tab + 1;
		}
		if (cols.size() < 3 || cols[0].empty())
			continue;
		Activation a;
		a.unixTime = std::strtoll(cols[1].c_str(), nullptr, 10);
		a.behavior = cols.size() > 2 ? std::atoi(cols[2].c_str()) : 0;
		a.mapId = cols.size() > 3 ? static_cast<uint32_t>(std::strtoul(cols[3].c_str(), nullptr, 10)) : 0;
		a.shardId = cols.size() > 4 ? static_cast<uint32_t>(std::strtoul(cols[4].c_str(), nullptr, 10)) : 0;
		a.instanceId = cols.size() > 5 ? static_cast<uint32_t>(std::strtoul(cols[5].c_str(), nullptr, 10)) : 0;
		a.resetLength = cols.size() > 6 ? static_cast<float>(std::atof(cols[6].c_str())) : 0.f;
		if (cols.size() > 7)
			a.character = cols[7];
		gStates[cols[0]] = std::move(a);
	}
	gDirty = false;
}

void MarkerBehaviors::Save(bool force)
{
	std::string body;
	{
		std::lock_guard<std::mutex> lock(gMu);
		if (!gDirty && !force)
			return;
		body.reserve(gStates.size() * 64 + 16);
		body += "v1\n";
		for (const auto& kv : gStates)
		{
			char row[512];
			std::snprintf(row, sizeof(row), "%s\t%lld\t%d\t%u\t%u\t%u\t%.3f\t%s\n",
				kv.first.c_str(),
				static_cast<long long>(kv.second.unixTime),
				kv.second.behavior,
				kv.second.mapId,
				kv.second.shardId,
				kv.second.instanceId,
				static_cast<double>(kv.second.resetLength),
				kv.second.character.c_str());
			body += row;
		}
		gDirty = false;
		gLastSave = GetTickCount();
	}
	WriteUtf8File(PathW(), body);
}

void MarkerBehaviors::ResetAllStates()
{
	{
		std::lock_guard<std::mutex> lock(gMu);
		gStates.clear();
		gDirty = true;
	}
	Save(true);
	Toast("Pathing marker states reset.");
}

