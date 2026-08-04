#include "MarkerBehaviors.h"
#include "MarkerBehaviorsInternal.h"

#include "HelperTheme.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <imgui.h>
#include <windows.h>

using namespace MarkerBehaviorsDetail;

void MarkerBehaviors::RequestInteract()
{
	gInteractReq.store(true, std::memory_order_release);
}

bool MarkerBehaviors::ShouldDisplay(const PathingTrails::Marker& m,
	uint32_t mapId, uint32_t shardId, uint32_t instanceId,
	const char* characterName)
{
	if (m.behavior == 0 || !m.guid[0])
		return true;
	std::lock_guard<std::mutex> lock(gMu);
	return !IsHiddenLocked(m, mapId, shardId, instanceId, characterName);
}

MarkerBehaviors::NearbyUi MarkerBehaviors::GetNearbyUi()
{
	std::lock_guard<std::mutex> lock(gMu);
	return gNearby;
}

void MarkerBehaviors::Tick(
	uint32_t mapId, uint32_t shardId, uint32_t instanceId,
	float avatarX, float avatarY, float avatarZ,
	const char* characterName,
	const std::vector<PathingTrails::Marker>& markers)
{
	const DWORD nowTick = GetTickCount();
	const bool wantInteract = gInteractReq.exchange(false, std::memory_order_acq_rel);
	if (nowTick - gLastTick < kTickMs && !wantInteract)
		return;
	gLastTick = nowTick;

	if (mapId != gLastMapId && gLastMapId != 0)
	{
		std::lock_guard<std::mutex> lock(gMu);
		for (auto it = gStates.begin(); it != gStates.end(); )
		{
			if (it->second.behavior == 1 && it->second.mapId == gLastMapId)
			{
				it = gStates.erase(it);
				gDirty = true;
			}
			else
				++it;
		}
	}
	gLastMapId = mapId;

	gPendingShow.clear();
	gPendingHide.clear();
	gPendingCopy.clear();
	gPendingCopyMsg.clear();
	gPendingInfo.clear();

	NearbyUi nearUi{};
	float bestTipDist = 12.f;
	float bestInteractDist = 1e30f;
	const PathingTrails::Marker* interactTarget = nullptr;
	std::unordered_set<std::string> firedGuids;

	for (const PathingTrails::Marker& m : markers)
	{
		if (!HasRuntime(m))
			continue;
		if (!ShouldDisplay(m, mapId, shardId, instanceId, characterName))
			continue;

		const float d = Dist3(avatarX, avatarY, avatarZ, m.world.x, m.world.y, m.world.z);
		if (!std::isfinite(d))
			continue;

		const float range = InteractRange(m);
		const bool inRange = d <= range;

		if (m.autoTrigger && m.triggerRange > 0.05f && inRange)
		{
			const bool needsFire = m.behavior != 0 || m.hide[0] || m.show[0] ||
				m.info[0] || m.copy[0];
			if (needsFire)
			{
				const std::string key = m.guid[0] ? std::string(m.guid) :
					(std::string(m.label) + "@" + std::to_string(static_cast<int>(d * 10)));
				if (!firedGuids.count(key))
				{
					firedGuids.insert(key);
					if (m.behavior != 0)
						QueueTrigger(m, mapId, shardId, instanceId, characterName);
					else if (m.guid[0])
						QueueTrigger(m, mapId, shardId, instanceId, characterName, 1);
					else
						QueueTrigger(m, mapId, shardId, instanceId, characterName);
				}
			}
		}

		if (inRange && d < bestInteractDist)
		{
			bestInteractDist = d;
			interactTarget = &m;
		}

		if ((m.tipDescription[0] || m.info[0] ||
			(m.tipName[0] && (std::strstr(m.label, ".bfs.") ||
				std::strstr(m.label, ".mount.") ||
				(m.iconId[0] && (std::strstr(m.iconId, "Mounts") ||
					std::strstr(m.iconId, "mounts")))))) &&
			d < bestTipDist)
		{
			bestTipDist = d;
			nearUi.valid = true;
			nearUi.distance = d;
			nearUi.canInteract = inRange &&
				(m.behavior != 0 || m.hide[0] || m.show[0] || m.info[0] || m.copy[0]);
			std::snprintf(nearUi.tipName, sizeof(nearUi.tipName), "%s",
				m.tipName[0] ? m.tipName : "Marker");
			std::snprintf(nearUi.tipDescription, sizeof(nearUi.tipDescription), "%s",
				m.tipDescription);
			if (m.info[0])
			{
				std::snprintf(nearUi.infoPreview, sizeof(nearUi.infoPreview), "%.140s%s",
					m.info, std::strlen(m.info) > 140 ? "…" : "");
			}
			if (nearUi.canInteract)
				std::snprintf(nearUi.status, sizeof(nearUi.status),
					"Interact · %.1fm", d);
			else
				std::snprintf(nearUi.status, sizeof(nearUi.status), "%.1fm", d);
		}
	}

	if (wantInteract && interactTarget)
		QueueTrigger(*interactTarget, mapId, shardId, instanceId, characterName);

	{
		std::lock_guard<std::mutex> lock(gMu);
		gNearby = nearUi;
	}

	FlushPending();

	if (nowTick - gLastSave > kSaveDebounceMs)
		Save(false);
}

void MarkerBehaviors::DrawOverlay()
{
	const NearbyUi ui = GetNearbyUi();
	const DWORD now = GetTickCount();

	if (ui.valid)
	{
		ImGui::SetNextWindowBgAlpha(0.82f);
		ImGui::SetNextWindowPos(
			ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.72f),
			ImGuiCond_Always, ImVec2(0.5f, 0.f));
		ImGui::SetNextWindowSizeConstraints(ImVec2(220.f, 0.f), ImVec2(420.f, 220.f));
		const ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings;
		if (ImGui::Begin("###gw2igh_marker_tip", nullptr, flags))
		{
			ImGui::TextColored(HelperTheme::Gold, "%s", ui.tipName);
			if (ui.tipDescription[0])
			{
				ImGui::PushTextWrapPos(0.f);
				ImGui::TextUnformatted(ui.tipDescription);
				ImGui::PopTextWrapPos();
			}
			if (ui.infoPreview[0])
			{
				ImGui::Spacing();
				ImGui::TextDisabled("%s", ui.infoPreview);
			}
			if (ui.status[0])
				ImGui::TextDisabled("%s", ui.status);
			if (ui.canInteract && ImGui::Button("Interact###gw2igh_marker_interact_btn"))
				RequestInteract();
		}
		ImGui::End();
	}

	if (gShowInfo && gInfoPopup[0])
	{
		ImGui::OpenPopup("Pathing info###gw2igh_marker_info");
		gShowInfo = false;
	}
	if (ImGui::BeginPopupModal("Pathing info###gw2igh_marker_info", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::PushTextWrapPos(440.f);
		ImGui::TextUnformatted(gInfoPopup);
		ImGui::PopTextWrapPos();
		ImGui::Spacing();
		if (ImGui::Button("OK###gw2igh_marker_info_ok", ImVec2(120.f, 0.f)))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	if (gToast[0] && now < gToastUntil)
	{
		ImGui::SetNextWindowBgAlpha(0.75f);
		ImGui::SetNextWindowPos(
			ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.18f),
			ImGuiCond_Always, ImVec2(0.5f, 0.f));
		if (ImGui::Begin("###gw2igh_marker_toast", nullptr,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
				ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
				ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::TextUnformatted(gToast);
		}
		ImGui::End();
	}
}

