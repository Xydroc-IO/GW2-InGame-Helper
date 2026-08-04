#include "PathingTrails.h"

#include "Globals.h"
#include "PathingIndex.h"
#include "PathingParse.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
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

	void QueueMapIcons(std::unordered_map<std::string, std::wstring>& assetsNeeded,
		const std::wstring& preferredPack, uint32_t epoch)
	{
		if (assetsNeeded.empty())
			return;

		/* Extract icons — trail chevrons first (missing → solid line), then Mounts,
		   then Numbers. Never rescan the whole 45MB zip (Wine OOM). */
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
		/* Lady map-completion inherits these — guarantee they upload. */
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
			nullptr
		};
		std::wstring mountPack = preferredPack;
		if (mountPack.empty())
			mountPack = assetsNeeded.begin()->second;
		if (!mountPack.empty())
		{
			for (int i = 0; kLadyTrailIcons[i]; ++i)
				assetsNeeded.emplace(kLadyTrailIcons[i], mountPack);
			for (int i = 0; kLadyMountIcons[i]; ++i)
				assetsNeeded.emplace(kLadyMountIcons[i], mountPack);
		}

		std::vector<std::pair<std::string, std::wstring>> iconList(
			assetsNeeded.begin(), assetsNeeded.end());
		std::sort(iconList.begin(), iconList.end(),
			[](const auto& a, const auto& b) {
				auto prio = [](const std::string& path) {
					const std::string low = ToLower(path);
					/* Paths first — cyan/white line means these never uploaded. */
					if (low.find("images/trails/") != std::string::npos ||
						low.find("/trails/") != std::string::npos ||
						low.find("trailarrow") != std::string::npos ||
						low.find("trail -") != std::string::npos ||
						low.find("trail_") != std::string::npos ||
						low.find("footprints") != std::string::npos)
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
				return a.second < b.second;
			});
		OpenPack iconPack;
		std::wstring iconPackPath;
		size_t queued = 0;
		for (const auto& kv : iconList)
		{
			if (queued >= 128 || gEpoch.load(std::memory_order_acquire) != epoch)
				break;
			{
				std::lock_guard<std::mutex> lock(gIconMutex);
				if (gIconQueued.count(kv.first))
					continue;
			}
			if (kv.second != iconPackPath)
			{
				iconPack.Close();
				iconPackPath = kv.second;
				if (!iconPack.Open(iconPackPath))
					continue;
			}
			std::string entry = kv.first;
			DecodeXmlEntities(entry);
			std::replace(entry.begin(), entry.end(), '\\', '/');
			while (entry.rfind("./", 0) == 0)
				entry.erase(0, 2);
			while (!entry.empty() && entry.front() == '/')
				entry.erase(entry.begin());
			int idx = ZipLocate(iconPack.zip, entry);
			if (idx < 0 && entry.rfind("Data/", 0) == 0)
				idx = ZipLocate(iconPack.zip, entry.substr(5));
			if (idx < 0)
				idx = ZipLocate(iconPack.zip, std::string("Data/") + entry);
			if (idx < 0 && entry.rfind("POIs/", 0) == 0)
				idx = ZipLocate(iconPack.zip, entry.substr(5));
			if (idx < 0)
				idx = ZipLocate(iconPack.zip, std::string("POIs/") + entry);
			if (idx < 0 && entry.rfind("Data/", 0) == 0)
				idx = ZipLocate(iconPack.zip, std::string("POIs/") + entry);
			std::vector<uint8_t> bytes;
			if (idx < 0 || !ZipExtractIndex(iconPack.zip, idx, bytes, 2u * 1024u * 1024u))
				continue;
			PendingIcon pending;
			pending.id = IconTextureId(kv.first);
			pending.bytes = std::move(bytes);
			{
				std::lock_guard<std::mutex> lock(gIconMutex);
				if (gPendingIcons.size() >= 256)
					break;
				gIconQueued[kv.first] = true;
				gPendingIcons.push_back(std::move(pending));
			}
			++queued;
		}
	}

} // namespace PathingDetail
