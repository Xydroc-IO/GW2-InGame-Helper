#include "ProgressDataInternal.h"

#include "CraftingData.h"
#include "Gw2Icons.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "MumbleIdentity.h"
#include "PadNav.h"
#include "WalletPad.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace ProgressDetail
{
	void DrawArmoryList(const Snapshot& snap)
	{
		PadNav::SectionTitle("Legendary Armory");
		if (snap.legs.empty() && !gBusy)
		{
			ImGui::TextWrapped("No catalog yet - click Refresh.");
			return;
		}

		int shown = 0;
		for (const LegRow& r : snap.legs)
		{
			if (!FilterMatch(r, gFilter)) continue;
			const bool have = r.owned > 0;
			if (gShowMode == 1 && have) continue;
			if (gShowMode == 2 && !have) continue;
			++shown;
			ImGui::PushID(r.id);

			const float rowY = ImGui::GetCursorScreenPos().y;
			if (Gw2Icons::ImageItem(r.id, 26.f))
				ImGui::SameLine(0.f, 8.f);
			else
			{
				/* Reserve icon slot so text lines stay aligned while icons resolve. */
				ImGui::Dummy(ImVec2(26.f, 26.f));
				ImGui::SameLine(0.f, 8.f);
			}

			const char* name = r.name.empty() ? "..." : r.name.c_str();
			if (have)
				ImGui::TextColored(HelperTheme::Ok, "%s", name);
			else
				ImGui::TextColored(HelperTheme::Ink, "%s", name);

			ImGui::SameLine();
			if (r.owned >= 0)
			{
				ImGui::TextColored(HelperTheme::GoldMuted,
					"%d/%d", r.owned, r.maxCount);
			}
			else
				ImGui::TextColored(HelperTheme::Muted, "#%d", r.id);

			ImGui::SameLine();
			if (ImGui::SmallButton("Plan"))
			{
				char idBuf[24];
				std::snprintf(idBuf, sizeof(idBuf), "%d", r.id);
				CraftingData::QueuePlan(idBuf);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Wiki"))
				OpenWikiItem(r.id, r.name);

			/* Soft gold rule under each row — plaque spacing without cards. */
			const float y2 = (std::max)(ImGui::GetCursorScreenPos().y, rowY + 28.f);
			(void)y2;
			ImGui::Spacing();
			ImGui::PopID();
		}
		if (shown == 0)
			ImGui::TextColored(HelperTheme::Muted, "No matches.");
	}

	void DrawCharacterRoster(const Snapshot& snap)
	{
		Gw2Icons::WarmProfessionIcons();
		PadNav::SectionTitle("Characters");
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Official roster art. Bags opens Wallet filtered to that toon. "
			"In-game swaps need Raidcore Fast Swap (Helper cannot log you in).");
		PadNav::PopWrap();

		if (!snap.hasKey)
		{
			ImGui::TextWrapped("Add an API key with the characters scope to list your roster.");
			return;
		}
		if (snap.chars.empty())
		{
			ImGui::TextColored(HelperTheme::Muted,
				snap.scopeFail
					? "Check API scopes (need characters)."
					: (gBusy ? "Loading roster..." : "No characters loaded yet — click Refresh."));
			return;
		}

		const char* active = MumbleIdentity::CharacterName();
		const size_t n = (std::min)(snap.chars.size(), kMaxCharDetails);
		for (size_t i = 0; i < n; ++i)
		{
			const CharRow& c = snap.chars[i];
			ImGui::PushID(static_cast<int>(i));

			const bool isActive = active && active[0] && c.name == active;
			const ImVec2 rowMin = ImGui::GetCursorScreenPos();
			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->ChannelsSplit(2);
			dl->ChannelsSetCurrent(1);

			if (!c.profession.empty() && Gw2Icons::ImageProfession(c.profession.c_str(), 28.f))
				ImGui::SameLine(0.f, 8.f);
			else
			{
				ImGui::Dummy(ImVec2(28.f, 28.f));
				ImGui::SameLine(0.f, 8.f);
			}

			ImGui::BeginGroup();
			if (isActive)
				ImGui::TextColored(ImVec4(1.f, 0.88f, 0.45f, 1.f), "%s", c.name.c_str());
			else
				ImGui::TextColored(HelperTheme::Ink, "%s", c.name.c_str());

			char meta[128];
			meta[0] = '\0';
			if (c.level >= 0 && !c.profession.empty() && !c.race.empty())
				std::snprintf(meta, sizeof(meta), "Lv %lld  ·  %s  ·  %s",
					c.level, c.profession.c_str(), c.race.c_str());
			else if (c.level >= 0 && !c.profession.empty())
				std::snprintf(meta, sizeof(meta), "Lv %lld  ·  %s", c.level, c.profession.c_str());
			else if (!c.profession.empty())
				std::snprintf(meta, sizeof(meta), "%s", c.profession.c_str());
			else if (c.level >= 0)
				std::snprintf(meta, sizeof(meta), "Lv %lld", c.level);
			if (meta[0])
				ImGui::TextColored(HelperTheme::Muted, "%s", meta);
			if (isActive)
				ImGui::TextColored(HelperTheme::Gold, "In world");
			ImGui::EndGroup();

			ImGui::SameLine();
			if (Gw2Ui::Image(Gw2Ui::Icon::Inventory, 16.f))
				ImGui::SameLine(0.f, 4.f);
			if (ImGui::SmallButton("Bags"))
				WalletPad::FocusCharacterBags(c.name.c_str());

			if (isActive)
			{
				const ImVec2 rowMax(
					ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x,
					ImGui::GetItemRectMax().y + 4.f);
				dl->ChannelsSetCurrent(0);
				dl->AddRectFilled(ImVec2(rowMin.x - 4.f, rowMin.y - 2.f), rowMax,
					IM_COL32(90, 70, 28, 70), 2.f);
				dl->AddRect(ImVec2(rowMin.x - 4.f, rowMin.y - 2.f), rowMax,
					IM_COL32(200, 160, 70, 140), 2.f);
			}
			dl->ChannelsMerge();
			ImGui::Spacing();
			ImGui::PopID();
		}
		if (snap.chars.size() > n)
		{
			ImGui::TextColored(HelperTheme::Muted,
				"Showing %d of %d", static_cast<int>(n), static_cast<int>(snap.chars.size()));
		}
	}
} // namespace ProgressDetail
