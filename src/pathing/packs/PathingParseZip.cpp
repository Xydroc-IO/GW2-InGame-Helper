#include "PathingParse.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

#include "miniz/miniz.h"

namespace PathingParse
{
/* ---- ZIP via miniz (TacO .taco is a zip) ---- */
bool ReadFileW(const std::wstring& path, std::vector<uint8_t>& out, size_t maxBytes)
{
	out.clear();
	FILE* f = _wfopen(path.c_str(), L"rb");
	if (!f)
		return false;
	std::fseek(f, 0, SEEK_END);
	const long sz = std::ftell(f);
	std::fseek(f, 0, SEEK_SET);
	if (sz <= 0 || static_cast<size_t>(sz) > maxBytes)
	{
		std::fclose(f);
		return false;
	}
	out.resize(static_cast<size_t>(sz));
	const bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
	std::fclose(f);
	if (!ok)
		out.clear();
	return ok;
}

/* Fast lookup via the zip central directory (binary search) instead of a
   linear scan per trail - critical to avoid a startup freeze with big packs. */
int ZipLocate(mz_zip_archive& zip, const std::string& entryName)
{
	std::string want = entryName;
	std::replace(want.begin(), want.end(), '\\', '/');
	int idx = mz_zip_reader_locate_file(&zip, want.c_str(), nullptr, 0);
	if (idx >= 0)
		return idx;
	/* Some packs store backslash separators - try that form too. */
	std::string alt = entryName;
	std::replace(alt.begin(), alt.end(), '/', '\\');
	return mz_zip_reader_locate_file(&zip, alt.c_str(), nullptr, 0);
}

bool ZipExtractIndex(mz_zip_archive& zip, int fileIndex,
	std::vector<uint8_t>& out, size_t maxOut)
{
	out.clear();
	if (fileIndex < 0)
		return false;
	mz_zip_archive_file_stat st{};
	if (!mz_zip_reader_file_stat(&zip, static_cast<mz_uint>(fileIndex), &st) ||
		st.m_is_directory)
		return false;
	if (st.m_uncomp_size == 0 || st.m_uncomp_size > maxOut)
		return false;
	size_t sz = 0;
	void* mem = mz_zip_reader_extract_to_heap(&zip, static_cast<mz_uint>(fileIndex), &sz, 0);
	if (!mem || sz == 0)
		return false;
	out.assign(static_cast<uint8_t*>(mem), static_cast<uint8_t*>(mem) + sz);
	mz_free(mem);
	return true;
}

bool ZipExtractEntry(mz_zip_archive& zip, const std::string& entryName,
	std::vector<uint8_t>& out, size_t maxOut)
{
	return ZipExtractIndex(zip, ZipLocate(zip, entryName), out, maxOut);
}

bool ZipReadEntry(const std::wstring& zipPath, const std::string& entryName,
	std::vector<uint8_t>& out, size_t maxOut)
{
	out.clear();
	std::vector<uint8_t> file;
	if (!ReadFileW(zipPath, file, kMaxZipBytes))
		return false;

	mz_zip_archive zip{};
	if (!mz_zip_reader_init_mem(&zip, file.data(), file.size(), 0))
		return false;
	const bool ok = ZipExtractEntry(zip, entryName, out, maxOut);
	mz_zip_reader_end(&zip);
	return ok;
}

/* Read only the .trl header (version + mapId = 8 bytes) via a streaming
   iterator so we never decompress the whole trail just to learn its map. */
uint32_t PeekTrlMapId(mz_zip_archive& zip, int fileIndex)
{
	if (fileIndex < 0)
		return 0;
	mz_zip_reader_extract_iter_state* it =
		mz_zip_reader_extract_iter_new(&zip, static_cast<mz_uint>(fileIndex), 0);
	if (!it)
		return 0;
	uint8_t hdr[8]{};
	const size_t got = mz_zip_reader_extract_iter_read(it, hdr, sizeof(hdr));
	mz_zip_reader_extract_iter_free(it);
	if (got < sizeof(hdr))
		return 0;
	uint32_t mid = 0;
	std::memcpy(&mid, hdr + 4, 4);
	if (mid == 0 || mid > 100000)
		return 0;
	return mid;
}

bool ParseTrl(const std::vector<uint8_t>& data, uint32_t& mapId,
	std::vector<PathingTrails::WorldPoint>& world)
{
	world.clear();
	if (data.size() < 20)
		return false;
	/* version (u32) + mapId (u32) + N * float3 (x,y,z) - Y up, horizontal = x,z.
	   TacO/Blish/Taimi: a (0,0,0) point ends a trail *section*. Connecting across
	   those breaks draws compass spaghetti (hub -> every next segment). */
	uint32_t ver = 0, mid = 0;
	std::memcpy(&ver, data.data(), 4);
	std::memcpy(&mid, data.data() + 4, 4);
	(void)ver;
	mapId = mid;
	if (mapId == 0 || mapId > 100000)
		return false;

	size_t off = 8;
	size_t rem = data.size() - 8;
	rem -= rem % 12;
	const size_t count = rem / 12;
	if (count < 2)
		return false;

	auto isBreak = [](float x, float y, float z) -> bool
	{
		return x == 0.f && y == 0.f && z == 0.f;
	};
	auto okPoint = [](float x, float y, float z) -> bool
	{
		return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) &&
			std::fabs(x) < 1.0e6f && std::fabs(y) < 1.0e6f && std::fabs(z) < 1.0e6f &&
			!(x == 0.f && y == 0.f && z == 0.f);
	};

	/* Collect every section first - Lady HP trails are multi-section and used to
	   drop everything after a flat 512-point budget (paths stopped mid-map). */
	std::vector<std::vector<PathingTrails::WorldPoint>> sections;
	sections.reserve(64);
	std::vector<PathingTrails::WorldPoint> section;
	section.reserve(64);
	size_t totalPts = 0;

	auto flushSection = [&]()
	{
		if (section.size() < 2)
		{
			section.clear();
			return;
		}
		totalPts += section.size();
		sections.push_back(std::move(section));
		section.clear();
		section.reserve(64);
	};

	for (size_t i = 0; i < count; ++i)
	{
		float x = 0.f, y = 0.f, z = 0.f;
		std::memcpy(&x, data.data() + off + i * 12, 4);
		std::memcpy(&y, data.data() + off + i * 12 + 4, 4);
		std::memcpy(&z, data.data() + off + i * 12 + 8, 4);
		if (isBreak(x, y, z))
		{
			flushSection();
			continue;
		}
		if (!okPoint(x, y, z))
			continue;
		section.push_back({x, y, z});
	}
	flushSection();
	if (sections.empty())
		return false;

	auto appendDecimated = [&](const std::vector<PathingTrails::WorldPoint>& src, size_t budget)
	{
		if (src.size() < 2 || budget < 2)
			return;
		if (!world.empty())
			world.push_back({NAN, NAN, NAN});
		if (src.size() <= budget)
		{
			world.insert(world.end(), src.begin(), src.end());
			return;
		}
		for (size_t k = 0; k < budget; ++k)
		{
			const size_t i = (k * (src.size() - 1)) / (budget - 1);
			world.push_back(src[i]);
		}
	};

	/* Leave room for NaN section breaks in the point budget. */
	const size_t breakBudget = sections.size() > 0 ? sections.size() - 1 : 0;
	size_t pointBudget = kMaxPointsPerTrail > breakBudget
		? (kMaxPointsPerTrail - breakBudget) : 2;
	world.reserve(std::min(totalPts + breakBudget, kMaxPointsPerTrail));

	if (totalPts <= pointBudget)
	{
		for (const auto& sec : sections)
			appendDecimated(sec, sec.size());
	}
	else
	{
		/* Spread budget across sections so later HP segments are not dropped. */
		size_t assigned = 0;
		for (size_t s = 0; s < sections.size(); ++s)
		{
			const size_t leftSecs = sections.size() - s;
			const size_t leftBudget = pointBudget > assigned ? (pointBudget - assigned) : 0;
			size_t share = std::max<size_t>(2, (sections[s].size() * pointBudget) / totalPts);
			if (share > leftBudget)
				share = leftBudget;
			/* Keep enough for remaining sections (2 pts each). */
			const size_t needRest = (leftSecs - 1) * 2;
			if (leftBudget > needRest && share > leftBudget - needRest)
				share = leftBudget - needRest;
			if (share < 2)
				share = leftBudget >= 2 ? 2 : leftBudget;
			appendDecimated(sections[s], share);
			assigned += share;
		}
	}
	return world.size() >= 2;
}

} // namespace PathingParse
