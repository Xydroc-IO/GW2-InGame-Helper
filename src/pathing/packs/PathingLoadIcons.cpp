#include "PathingTrails.h"

#include "Globals.h"
#include "PathingIndex.h"
#include "PathingParse.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <windows.h>
#include "miniz/miniz.h"

namespace PathingDetail
{

	std::string IconTextureId(const std::string& iconFile)
	{
		std::string id = "TW_ICO_";
		for (char c : iconFile)
		{
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9'))
				id += c;
			else
				id += '_';
		}
		if (id.size() > 120)
			id.resize(120);
		return id;
	}

	static bool LooksLikeLadyPack(const std::wstring& path)
	{
		const std::wstring low = [&]() {
			std::wstring s = path;
			for (wchar_t& c : s)
				if (c >= L'A' && c <= L'Z')
					c = static_cast<wchar_t>(c - L'A' + L'a');
			return s;
		}();
		return low.find(L"ladyelyssa") != std::wstring::npos;
	}

	static bool IsTrailTexturePath(const std::string& low)
	{
		return low.find("images/trails/") != std::string::npos ||
			low.find("/trails/") != std::string::npos ||
			low.find("trailarrow") != std::string::npos ||
			low.find("trail -") != std::string::npos ||
			low.find("trail_") != std::string::npos ||
			low.find("footprints") != std::string::npos ||
			low.find("line - heart") != std::string::npos ||
			low.find("line-heart") != std::string::npos ||
			low.find("images/line") != std::string::npos ||
			low.find("hp trail") != std::string::npos;
	}

	static int LocateIconEntry(mz_zip_archive& zip, std::string entry)
	{
		DecodeXmlEntities(entry);
		std::replace(entry.begin(), entry.end(), '\\', '/');
		while (entry.rfind("./", 0) == 0)
			entry.erase(0, 2);
		while (!entry.empty() && entry.front() == '/')
			entry.erase(entry.begin());
		int idx = ZipLocate(zip, entry);
		if (idx < 0 && entry.rfind("Data/", 0) == 0)
			idx = ZipLocate(zip, entry.substr(5));
		if (idx < 0)
			idx = ZipLocate(zip, std::string("Data/") + entry);
		if (idx < 0 && entry.rfind("POIs/", 0) == 0)
			idx = ZipLocate(zip, entry.substr(5));
		if (idx < 0)
			idx = ZipLocate(zip, std::string("POIs/") + entry);
		if (idx < 0 && entry.rfind("Data/", 0) == 0)
			idx = ZipLocate(zip, std::string("POIs/") + entry);
		return idx;
	}

	void QueueMapIcons(std::unordered_map<std::string, std::wstring>& assetsNeeded,
		const std::wstring& preferredPack, uint32_t epoch)
	{
		if (assetsNeeded.empty() && preferredPack.empty())
			return;

		/* Extract icons — trail chevrons first (missing → solid tinted ribbon). */
		static const char* kLadyMountIcons[] = {
			"Data/Images/Mounts/Mount_Raptor.png",
			"Data/Images/Mounts/Mount_Springer.png",
			"Data/Images/Mounts/Mount_Skimmer.png",
			"Data/Images/Mounts/Mount_Jackal.png",
			"Data/Images/Mounts/Mount_Griffon.png",
			"Data/Images/Mounts/Mount_Beetle.png",
			"Data/Images/Mounts/Mount_Warclaw.png",
			"Data/Images/Mounts/Mount_Skyscale.png",
			"Data/Images/Mounts/Dismount.png",
			"Data/Images/Mounts/Leap.png",
			"Data/Images/Mounts/Hover.png",
			nullptr
		};
		/* Lady map-completion inherits these — force onto a Lady pack when possible. */
		static const char* kLadyTrailIcons[] = {
			"Data/Images/Trails/White Arrow Black Border.png",
			"Data/Images/Trails/Footprints.png",
			"Data/Images/Trails/Trail Pointer - Small.png",
			"Data/Images/Trails/Dashed Lines - Fine with Shadow.png",
			"Data/Images/Trail - Stubby.png",
			"Data/Images/trailarrow.png",
			"Data/Images/trailarrow-small.png",
			"Data/Images/trailarrow-two-tone.png",
			"Data/Images/Line - Heart.png",
			"Data/Images/Line - Heart Dark.png",
			nullptr
		};

		std::wstring ladyPack;
		std::wstring mountPack = preferredPack;
		std::vector<std::wstring> allPacks;
		allPacks.reserve(8);
		auto notePack = [&](const std::wstring& p) {
			if (p.empty())
				return;
			for (const auto& e : allPacks)
				if (e == p)
					return;
			allPacks.push_back(p);
			if (ladyPack.empty() && LooksLikeLadyPack(p))
				ladyPack = p;
		};
		for (const auto& kv : assetsNeeded)
			notePack(kv.second);
		notePack(preferredPack);
		if (mountPack.empty() && !assetsNeeded.empty())
			mountPack = assetsNeeded.begin()->second;
		if (ladyPack.empty())
			ladyPack = mountPack;

		for (int i = 0; kLadyTrailIcons[i]; ++i)
		{
			const char* path = kLadyTrailIcons[i];
			auto it = assetsNeeded.find(path);
			if (it != assetsNeeded.end())
				notePack(it->second);
			else
				assetsNeeded[path] = !ladyPack.empty() ? ladyPack : mountPack;
		}
		if (!mountPack.empty())
		{
			for (int i = 0; kLadyMountIcons[i]; ++i)
				assetsNeeded.emplace(kLadyMountIcons[i], mountPack);
		}

		std::vector<std::pair<std::string, std::wstring>> iconList(
			assetsNeeded.begin(), assetsNeeded.end());
		std::sort(iconList.begin(), iconList.end(),
			[](const auto& a, const auto& b) {
				auto prio = [](const std::string& path) {
					const std::string low = ToLower(path);
					if (IsTrailTexturePath(low))
						return 0;
					if (low.find("images/mounts/") != std::string::npos)
						return 1;
					if (low.find("images/numbers/") != std::string::npos)
						return 2;
					return 3;
				};
				const int pa = prio(a.first);
				const int pb = prio(b.first);
				if (pa != pb)
					return pa < pb;
				return a.first < b.first;
			});

		OpenPack iconPack;
		std::wstring iconPackPath;
		size_t queued = 0;
		for (const auto& kv : iconList)
		{
			if (queued >= 160 || gEpoch.load(std::memory_order_acquire) != epoch)
				break;
			{
				std::lock_guard<std::mutex> lock(gIconMutex);
				if (gIconQueued.count(kv.first))
					continue;
			}

			std::vector<std::wstring> tryPacks;
			tryPacks.push_back(kv.second);
			/* Heart / Lady line textures often get force-mapped to the wrong pack —
			   fall back across every pack we already opened for this map. */
			if (IsTrailTexturePath(ToLower(kv.first)))
			{
				if (!ladyPack.empty())
					tryPacks.push_back(ladyPack);
				for (const auto& p : allPacks)
					tryPacks.push_back(p);
			}
			std::unordered_set<std::wstring> seen;
			bool got = false;
			for (const std::wstring& packPath : tryPacks)
			{
				if (packPath.empty() || !seen.insert(packPath).second)
					continue;
				if (packPath != iconPackPath)
				{
					iconPack.Close();
					iconPackPath = packPath;
					if (!iconPack.Open(iconPackPath))
						continue;
				}
				const int idx = LocateIconEntry(iconPack.zip, kv.first);
				std::vector<uint8_t> bytes;
				if (idx < 0 || !ZipExtractIndex(iconPack.zip, idx, bytes, 2u * 1024u * 1024u))
					continue;

				PendingIcon pending;
				pending.id = IconTextureId(kv.first);
				pending.bytes = std::move(bytes);
				{
					std::lock_guard<std::mutex> lock(gIconMutex);
					if (gPendingIcons.size() >= 256)
						return;
					gIconQueued[kv.first] = true;
					gPendingIcons.push_back(std::move(pending));
				}
				++queued;
				got = true;
				break;
			}
			(void)got;
		}
	}

} // namespace PathingDetail
