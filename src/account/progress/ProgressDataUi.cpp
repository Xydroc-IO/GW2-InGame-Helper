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
#include <string>
#include <unordered_map>
#include <vector>

namespace ProgressDetail
{
	void DrawLegRow(const LegRow& r)
	{
		ImGui::PushID(r.id);

		if (Gw2Icons::ImageItem(r.id, 26.f))
			ImGui::SameLine(0.f, 8.f);
		else
		{
			ImGui::Dummy(ImVec2(26.f, 26.f));
			ImGui::SameLine(0.f, 8.f);
		}

		ImGui::BeginGroup();
		const char* name = r.name.empty() ? "..." : r.name.c_str();
		if (r.owned > 0)
			ImGui::TextColored(HelperTheme::Ok, "%s", name);
		else
			ImGui::TextColored(HelperTheme::Ink, "%s", name);

		char sub[96];
		sub[0] = '\0';
		const bool genShown = !r.generation.empty() && r.generation != "Other";
		if (!r.itemType.empty() && genShown)
			std::snprintf(sub, sizeof(sub), "%s  ·  %s", r.itemType.c_str(), r.generation.c_str());
		else if (!r.itemType.empty())
			std::snprintf(sub, sizeof(sub), "%s", r.itemType.c_str());
		else if (genShown)
			std::snprintf(sub, sizeof(sub), "%s", r.generation.c_str());
		if (sub[0])
			ImGui::TextColored(HelperTheme::Muted, "%s", sub);
		ImGui::EndGroup();

		ImGui::SameLine();
		if (r.owned >= 0)
			ImGui::TextColored(HelperTheme::GoldMuted, "%d/%d", r.owned, r.maxCount);
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

		ImGui::Spacing();
		ImGui::PopID();
	}

	bool BeginArmoryFold(const char* id, const char* label, bool startOpen)
	{
		ImGui::PushID(id);
		ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::GoldMuted);
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
		if (startOpen)
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		const bool open = ImGui::TreeNodeEx(label, flags);
		ImGui::PopStyleColor();
		if (!open)
			ImGui::PopID();
		return open;
	}

	void EndArmoryFold()
	{
		ImGui::TreePop();
		ImGui::PopID();
	}

	void DrawArmoryRowsOrGens(const std::vector<const LegRow*>& rows)
	{
		const bool searching = gFilter[0] != '\0';
		const bool splitGen = !searching && gGenFilter == 0 && rows.size() > 8;
		if (!splitGen)
		{
			for (const LegRow* r : rows)
				DrawLegRow(*r);
			return;
		}

		std::unordered_map<std::string, std::vector<const LegRow*>> byGen;
		for (const LegRow* r : rows)
		{
			const char* g = r->generation.empty() ? "Other" : r->generation.c_str();
			byGen[g].push_back(r);
		}
		for (int gi = 1; gi < kArmoryGenCount; ++gi)
		{
			const char* gen = kArmoryGens[gi];
			auto it = byGen.find(gen);
			if (it == byGen.end() || it->second.empty()) continue;
			int gOwned = 0;
			for (const LegRow* r : it->second)
				if (r->owned > 0) ++gOwned;
			char gHead[96];
			if (gShowMode == 0)
				std::snprintf(gHead, sizeof(gHead), "%s  %d / %d", gen, gOwned,
					static_cast<int>(it->second.size()));
			else
				std::snprintf(gHead, sizeof(gHead), "%s  %d", gen,
					static_cast<int>(it->second.size()));
			const bool genOpen = it->second.size() <= 12 || searching;
			if (!BeginArmoryFold(gen, gHead, genOpen))
				continue;
			for (const LegRow* r : it->second)
				DrawLegRow(*r);
			EndArmoryFold();
		}
	}

	void DrawArmoryGroup(const char* cat, const std::vector<const LegRow*>& rows, bool fold)
	{
		if (rows.empty()) return;

		int owned = 0;
		for (const LegRow* r : rows)
			if (r->owned > 0) ++owned;

		char head[96];
		if (gShowMode == 0)
			std::snprintf(head, sizeof(head), "%s  %d / %d", cat, owned, static_cast<int>(rows.size()));
		else
			std::snprintf(head, sizeof(head), "%s  %d", cat, static_cast<int>(rows.size()));

		if (!fold)
		{
			ImGui::TextColored(HelperTheme::GoldMuted, "%s", head);
			ImGui::Spacing();
			DrawArmoryRowsOrGens(rows);
			return;
		}

		const bool searching = gFilter[0] != '\0';
		const bool giant = std::strcmp(cat, "Weapon") == 0 || std::strcmp(cat, "Armor") == 0;
		if (!BeginArmoryFold(cat, head, searching || !giant))
			return;
		DrawArmoryRowsOrGens(rows);
		EndArmoryFold();
	}

	void DrawArmoryList(const Snapshot& snap)
	{
		PadNav::SectionTitle("Legendary Armory");
		if (snap.legs.empty() && !gBusy)
		{
			ImGui::TextWrapped("No catalog yet - click Refresh.");
			return;
		}

		std::vector<const LegRow*> vis;
		vis.reserve(snap.legs.size());
		for (const LegRow& r : snap.legs)
		{
			if (RowVisible(r))
				vis.push_back(&r);
		}
		if (vis.empty())
		{
			ImGui::TextColored(HelperTheme::Muted, "No matches.");
			return;
		}

		std::sort(vis.begin(), vis.end(), [](const LegRow* a, const LegRow* b) {
			return a->name < b->name;
		});

		std::unordered_map<std::string, std::vector<const LegRow*>> groups;
		for (const LegRow* r : vis)
		{
			const char* c = r->category.empty() ? "Other" : r->category.c_str();
			groups[c].push_back(r);
		}

		const bool foldCats = gCatFilter == 0;
		for (int i = 1; i < kArmoryCatCount; ++i)
			DrawArmoryGroup(kArmoryCats[i], groups[kArmoryCats[i]], foldCats);
		DrawArmoryGroup("Other", groups["Other"], foldCats);
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
