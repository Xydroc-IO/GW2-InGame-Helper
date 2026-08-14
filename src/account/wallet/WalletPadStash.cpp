#include "WalletShared.h"

#include "Gw2Icons.h"
#include "HelperTheme.h"
#include "PadLayout.h"
#include "PadNav.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace WalletDetail
{
	struct StashRow
	{
		const Entry* e = nullptr;
		const LocQty* loc = nullptr;
	};

	bool MatchesFilter(const Entry& e, const char* filter, int locFilter)
	{
		if (locFilter > 0)
		{
			const LocKind want = static_cast<LocKind>(locFilter - 1);
			bool any = false;
			for (const LocQty& l : e.locs)
			{
				if (l.kind == want) { any = true; break; }
			}
			if (!any) return false;
		}
		if (!filter || !filter[0]) return true;
		auto has = [](const char* hay, const char* needle) -> bool {
			if (!hay || !needle || !needle[0]) return true;
			const size_t nlen = std::strlen(needle);
			for (const char* p = hay; *p; ++p)
			{
				size_t i = 0;
				while (i < nlen)
				{
					const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(p[i])));
					const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(needle[i])));
					if (!p[i] || a != b) break;
					++i;
				}
				if (i == nlen) return true;
			}
			return false;
		};
		if (has(e.name.c_str(), filter)) return true;
		char idBuf[24];
		std::snprintf(idBuf, sizeof(idBuf), "%d", e.id);
		if (has(idBuf, filter)) return true;
		for (const LocQty& l : e.locs)
			if (has(l.where.c_str(), filter)) return true;
		return false;
	}

	void CollectStashRows(const Snapshot& snap, const char* filter, int locFilter,
		std::vector<StashRow>& out)
	{
		out.clear();
		for (const Entry& e : snap.entries)
		{
			if (!MatchesFilter(e, filter, locFilter))
				continue;
			for (const LocQty& l : e.locs)
			{
				if (locFilter > 0 && l.kind != static_cast<LocKind>(locFilter - 1))
					continue;
				out.push_back({ &e, &l });
			}
		}
	}

	void SortStashRows(std::vector<StashRow>& rows, int sortMode)
	{
		std::sort(rows.begin(), rows.end(),
			[sortMode](const StashRow& a, const StashRow& b) {
				if (sortMode == 1)
				{
					const int n = _stricmp(a.e->name.c_str(), b.e->name.c_str());
					if (n != 0) return n < 0;
					return a.loc->where < b.loc->where;
				}
				if (a.loc->count != b.loc->count)
					return a.loc->count > b.loc->count;
				return _stricmp(a.e->name.c_str(), b.e->name.c_str()) < 0;
			});
	}

	void DrawStashRow(const StashRow& row, int locFilter, bool hideCharChip)
	{
		const Entry& e = *row.e;
		const LocQty& loc = *row.loc;
		const int rowKey = e.isCurrency ? -e.id : e.id;
		ImGui::PushID(rowKey);
		ImGui::PushID(loc.where.c_str());

		const std::string qty = e.isCurrency && e.id == 1
			? FormatCoins(loc.count)
			: FormatCount(loc.count);

		static std::unordered_map<std::string, bool> sOpen;
		char openKey[192];
		std::snprintf(openKey, sizeof(openKey), "%d/%s", rowKey, loc.where.c_str());
		bool& open = sOpen[openKey];

		constexpr float kIcon = 22.f;
		const float lineH = (std::max)(kIcon, ImGui::GetTextLineHeightWithSpacing());
		const ImVec2 rowP = ImGui::GetCursorScreenPos();
		const float rowW = ImGui::GetContentRegionAvail().x;
		if (open)
		{
			ImGui::GetWindowDrawList()->AddRectFilled(rowP,
				ImVec2(rowP.x + rowW, rowP.y + lineH),
				ImGui::GetColorU32(HelperTheme::Header), 3.f);
		}

		if (e.isCurrency)
			Gw2Icons::ImageCurrency(e.id, kIcon);
		else
			Gw2Icons::ImageItem(e.id, kIcon);
		ImGui::SameLine(0.f, 6.f);

		const char* chip = (loc.kind == Loc_Character && !hideCharChip)
			? loc.where.c_str() : nullptr;
		const ImVec4 qtyCol = (e.isCurrency && e.id == 1)
			? HelperTheme::GoldBright : HelperTheme::Ink;
		PadLayout::TitleRow(chip, HelperTheme::Header, HelperTheme::GoldMuted,
			e.name.empty() ? "Item" : e.name.c_str(), qty.c_str(), qtyCol);

		ImGui::SetCursorScreenPos(rowP);
		if (ImGui::InvisibleButton("##stash_row", ImVec2(rowW, lineH)))
			open = !open;

		if (open)
		{
			ImGui::Indent();
			ImGui::TextColored(HelperTheme::Muted,
				"%s #%d", e.isCurrency ? "Currency" : "Item", e.id);
			for (const LocQty& l : e.locs)
			{
				if (locFilter > 0 && l.kind != static_cast<LocKind>(locFilter - 1))
					continue;
				const std::string lq = e.isCurrency && e.id == 1
					? FormatCoins(l.count)
					: FormatCount(l.count);
				PadLayout::NameAndValue(l.where.c_str(), lq.c_str(), HelperTheme::Muted);
			}
			ImGui::Unindent();
		}
		ImGui::PopID();
		ImGui::PopID();
	}

	constexpr ImGuiTreeNodeFlags kFold = ImGuiTreeNodeFlags_SpanAvailWidth;

	bool BeginFold(const char* id, const char* label)
	{
		ImGui::PushID(id);
		ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::GoldMuted);
		const bool open = ImGui::TreeNodeEx(label, kFold);
		ImGui::PopStyleColor();
		if (!open)
			ImGui::PopID();
		return open;
	}

	void EndFold()
	{
		ImGui::TreePop();
		ImGui::PopID();
	}

	void DrawCharFolds(const std::vector<StashRow>& rows, int locFilter)
	{
		std::vector<std::string> names;
		names.reserve(16);
		for (const StashRow& r : rows)
		{
			if (r.loc->kind != Loc_Character)
				continue;
			if (std::find(names.begin(), names.end(), r.loc->where) == names.end())
				names.push_back(r.loc->where);
		}
		std::sort(names.begin(), names.end(),
			[](const std::string& a, const std::string& b) {
				return _stricmp(a.c_str(), b.c_str()) < 0;
			});
		for (const std::string& name : names)
		{
			int nToon = 0;
			for (const StashRow& r : rows)
			{
				if (r.loc->kind == Loc_Character && r.loc->where == name)
					++nToon;
			}
			char head[96];
			std::snprintf(head, sizeof(head), "%s  ·  %d", name.c_str(), nToon);
			if (!BeginFold(name.c_str(), head))
				continue;
			for (const StashRow& r : rows)
			{
				if (r.loc->kind != Loc_Character || r.loc->where != name)
					continue;
				DrawStashRow(r, locFilter, true);
			}
			EndFold();
		}
	}

	void DrawEmptyStash(const Snapshot& snap, const char* filter, int locFilter)
	{
		PadNav::PushWrap();
		if (!snap.ok)
			ImGui::TextWrapped("No data yet - click Refresh.");
		else if (locFilter == 5)
		{
			if (snap.charCount <= 0)
				ImGui::TextWrapped(
					"No character bags indexed. Enable the characters + inventories "
					"scopes on your API key, then click Refresh.");
			else if (snap.charBagsOk <= 0)
				ImGui::TextWrapped(
					"%d characters listed but bag fetch failed. Check inventories "
					"scope, then Refresh.", snap.charCount);
			else if (snap.characterLocItems <= 0)
				ImGui::TextWrapped(
					"Character bag HTTP ok but no items parsed — click Refresh. "
					"If this persists after a reload, check inventories scope.");
			else if (filter && filter[0])
				ImGui::TextWrapped("No matches in character bags for that filter.");
			else
				ImGui::TextWrapped("No character-bag items match.");
		}
		else if ((filter && filter[0]) || locFilter > 0)
			ImGui::TextWrapped("No matches. Clear the filter or pick All locations.");
		else
			ImGui::TextWrapped("Stash is empty.");
		PadNav::PopWrap();
	}

	void DrawStashFolds(const Snapshot& snap, const char* filter, int locFilter, int sortMode)
	{
		std::vector<StashRow> rows;
		CollectStashRows(snap, filter, locFilter, rows);
		SortStashRows(rows, sortMode);
		const int shown = static_cast<int>(rows.size());
		if (shown > 0)
		{
			ImGui::TextColored(HelperTheme::Muted, "%d stacks", shown);
			PadLayout::NameAndValue("Item", "Qty", HelperTheme::GoldMuted);
		}
		PadLayout::BeginList("###gw2igh_wallet_list", 80.f);
		if (locFilter == 0)
		{
			for (int kind = 0; kind < Loc_Count; ++kind)
			{
				int nKind = 0;
				for (const StashRow& r : rows)
				{
					if (r.loc->kind == kind)
						++nKind;
				}
				if (nKind <= 0)
					continue;
				char head[64];
				std::snprintf(head, sizeof(head), "%s  ·  %d", kLocLabels[kind + 1], nKind);
				if (!BeginFold(kLocLabels[kind + 1], head))
					continue;
				if (kind == Loc_Character)
					DrawCharFolds(rows, locFilter);
				else
				{
					for (const StashRow& r : rows)
					{
						if (r.loc->kind != kind)
							continue;
						DrawStashRow(r, locFilter, false);
					}
				}
				EndFold();
			}
		}
		else if (locFilter == 5)
			DrawCharFolds(rows, locFilter);
		else
		{
			for (const StashRow& r : rows)
				DrawStashRow(r, locFilter, false);
		}
		if (shown == 0 && !gBusy)
			DrawEmptyStash(snap, filter, locFilter);
		PadLayout::EndList();
	}
} // namespace WalletDetail
