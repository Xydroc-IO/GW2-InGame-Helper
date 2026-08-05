#include "TrailToolsTrl.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#include <windows.h>

namespace
{
	std::wstring Utf8ToWide(const std::string& u)
	{
		if (u.empty())
			return {};
		const int n = MultiByteToWideChar(CP_UTF8, 0, u.c_str(), -1, nullptr, 0);
		if (n <= 0)
			return {};
		std::wstring w(static_cast<size_t>(n - 1), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, u.c_str(), -1, w.data(), n);
		return w;
	}
}

bool TrailToolsTrl::Write(const std::wstring& path, uint32_t mapId,
	const std::vector<PathingTrails::WorldPoint>& points)
{
	if (path.empty() || mapId == 0)
		return false;
	HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;

	uint32_t ver = 0;
	DWORD written = 0;
	bool ok = WriteFile(h, &ver, 4, &written, nullptr) && written == 4;
	ok = ok && WriteFile(h, &mapId, 4, &written, nullptr) && written == 4;
	for (const auto& p : points)
	{
		float trip[3] = { p.x, p.y, p.z };
		ok = ok && WriteFile(h, trip, 12, &written, nullptr) && written == 12;
		if (!ok)
			break;
	}
	CloseHandle(h);
	return ok;
}

bool TrailToolsTrl::Read(const std::wstring& path, uint32_t& mapId,
	std::vector<PathingTrails::WorldPoint>& points)
{
	points.clear();
	mapId = 0;
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	LARGE_INTEGER sz{};
	if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 20)
	{
		CloseHandle(h);
		return false;
	}
	std::vector<uint8_t> data(static_cast<size_t>(sz.QuadPart));
	DWORD got = 0;
	const bool readOk = ReadFile(h, data.data(), static_cast<DWORD>(data.size()), &got, nullptr);
	CloseHandle(h);
	if (!readOk || got < 20)
		return false;

	uint32_t ver = 0;
	std::memcpy(&ver, data.data(), 4);
	(void)ver;
	std::memcpy(&mapId, data.data() + 4, 4);
	if (mapId == 0 || mapId > 100000)
		return false;

	size_t rem = data.size() - 8;
	rem -= rem % 12;
	const size_t count = rem / 12;
	points.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		float x = 0.f, y = 0.f, z = 0.f;
		std::memcpy(&x, data.data() + 8 + i * 12, 4);
		std::memcpy(&y, data.data() + 8 + i * 12 + 4, 4);
		std::memcpy(&z, data.data() + 8 + i * 12 + 8, 4);
		points.push_back({ x, y, z });
	}
	return points.size() >= 2;
}

bool TrailToolsTrl::WriteUtf8(const std::string& pathUtf8, uint32_t mapId,
	const std::vector<PathingTrails::WorldPoint>& points)
{
	return Write(Utf8ToWide(pathUtf8), mapId, points);
}
