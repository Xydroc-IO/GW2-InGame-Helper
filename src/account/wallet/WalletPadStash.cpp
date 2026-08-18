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

	bool BeginFold(const char* id, const char* label, bool startOpen = false)
	{
		ImGui::PushID(id);
		ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::GoldMuted);
		ImGuiTreeNodeFlags flags = kFold;
		if (startOpen)
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		const bool open = ImGui::TreeNodeEx(label, flags);
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
		else if (locFilter == 2)
			ImGui::TextWrapped(
				"Materials storage did not load. Click Refresh. "
				"The API key needs the inventories scope.");
		else if ((filter && filter[0]) || locFilter > 0)
			ImGui::TextWrapped("No matches. Clear the filter or pick All locations.");
		else
			ImGui::TextWrapped("Stash is empty.");
		PadNav::PopWrap();
	}

	void DrawBankSlot(const SlotCell& c, float sz, int slotIndex)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 p0 = ImGui::GetCursorScreenPos();
		const ImVec2 p1(p0.x + sz, p0.y + sz);
		ImGui::PushID(slotIndex);
		ImGui::InvisibleButton("###gw2igh_stash_slot", ImVec2(sz, sz));
		const ImVec2 after = ImGui::GetCursorScreenPos();
		dl->AddRectFilled(p0, p1, ImGui::GetColorU32(HelperTheme::Child), 2.f);
		dl->AddRect(p0, p1, ImGui::GetColorU32(HelperTheme::Border), 2.f);
		if (c.id > 0)
		{
			const bool ghost = c.count <= 0;
			ImGui::SetCursorScreenPos(ImVec2(p0.x + 2.f, p0.y + 2.f));
			if (ghost)
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.28f);
			if (!Gw2Icons::ImageItem(c.id, sz - 4.f))
				ImGui::Dummy(ImVec2(sz - 4.f, sz - 4.f));
			if (ghost)
				ImGui::PopStyleVar();
			if (c.count > 1)
			{
				char n[16];
				std::snprintf(n, sizeof(n), "%d", c.count);
				const ImVec2 ts = ImGui::CalcTextSize(n);
				const ImVec2 tp(p1.x - ts.x - 3.f, p0.y + 1.f);
				dl->AddText(ImVec2(tp.x + 1.f, tp.y + 1.f), IM_COL32(0, 0, 0, 180), n);
				dl->AddText(tp, ImGui::GetColorU32(HelperTheme::Ink), n);
			}
			if (ImGui::IsMouseHoveringRect(p0, p1))
			{
				char name[160];
				if (!Gw2Icons::ItemName(c.id, name, sizeof(name)))
				{
					const std::string cached = LookupName(c.id, c.id, false);
					std::snprintf(name, sizeof(name), "%s",
						cached.empty() ? "Item" : cached.c_str());
				}
				if (c.count > 0)
					ImGui::SetTooltip("%s\n%d", name, c.count);
				else
					ImGui::SetTooltip("%s\nNot collected", name);
			}
		}
		ImGui::SetCursorScreenPos(after);
		ImGui::PopID();
	}

	bool SlotMatches(const SlotCell& c, const char* filter)
	{
		if (!filter || !filter[0])
			return true;
		if (c.id <= 0)
			return false;
		Entry e;
		e.id = c.id;
		char name[160];
		if (Gw2Icons::ItemName(c.id, name, sizeof(name)))
			e.name = name;
		else
			e.name = LookupName(c.id, c.id, false);
		return MatchesFilter(e, filter, 0);
	}

	void DrawSlotGrid(const std::vector<SlotCell>& slots, const char* filter)
	{
		const float sz = 36.f;
		const float gap = 3.f;
		const float cell = sz + gap;
		ImGui::PushTextWrapPos(-1.f);
		const float avail = ImGui::GetContentRegionAvail().x;
		int cols = static_cast<int>((avail + gap) / cell);
		if (cols < 4)
			cols = 4;
		if (cols > 10)
			cols = 10;
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		int drawn = 0;
		int idx = 0;
		for (const SlotCell& c : slots)
		{
			++idx;
			if (filter && filter[0] && !SlotMatches(c, filter))
				continue;
			const int col = drawn % cols;
			const int row = drawn / cols;
			ImGui::SetCursorScreenPos(ImVec2(origin.x + col * cell, origin.y + row * cell));
			DrawBankSlot(c, sz, idx);
			++drawn;
		}
		const int rows = drawn <= 0 ? 0 : (drawn + cols - 1) / cols;
		ImGui::SetCursorScreenPos(origin);
		if (rows > 0)
			ImGui::Dummy(ImVec2(cols * cell - gap, rows * cell - gap));
		ImGui::PopTextWrapPos();
	}

	bool CurrencyMatches(const Entry& e, const char* filter)
	{
		return e.isCurrency && MatchesFilter(e, filter, 0);
	}

	bool DrawWalletCurrencies(const Snapshot& snap, const char* filter)
	{
		std::vector<const Entry*> cur;
		cur.reserve(64);
		for (const Entry& e : snap.entries)
		{
			if (CurrencyMatches(e, filter))
				cur.push_back(&e);
		}
		if (cur.empty())
			return false;
		std::sort(cur.begin(), cur.end(),
			[](const Entry* a, const Entry* b) {
				if (a->id == 1)
					return b->id != 1;
				if (b->id == 1)
					return false;
				return _stricmp(a->name.c_str(), b->name.c_str()) < 0;
			});
		char head[64];
		std::snprintf(head, sizeof(head), "Wallet  ·  %d", static_cast<int>(cur.size()));
		if (!BeginFold("Wallet", head, true))
			return true;
		for (const Entry* e : cur)
		{
			ImGui::PushID(-e->id);
			const float xLine = ImGui::GetCursorPosX();
			const float yLine = ImGui::GetCursorPosY();
			constexpr float kIcon = 22.f;
			if (!Gw2Icons::ImageCurrency(e->id, kIcon))
				ImGui::Dummy(ImVec2(kIcon, kIcon));
			ImGui::SameLine(0.f, 8.f);
			const std::string amt = e->id == 1
				? FormatCoins(e->total)
				: FormatCount(e->total);
			const char* nm = e->name.empty() ? "Currency" : e->name.c_str();
			PadLayout::TitleRow(nullptr, HelperTheme::Header, HelperTheme::GoldMuted,
				nm, amt.c_str(), e->id == 1 ? HelperTheme::GoldBright : HelperTheme::Ink);
			const float rowH = (std::max)(kIcon, ImGui::GetTextLineHeightWithSpacing());
			ImGui::SetCursorPos(ImVec2(xLine, yLine + rowH));
			ImGui::PopID();
		}
		EndFold();
		return true;
	}

	void DrawStashFolds(const Snapshot& snap, const char* filter, int locFilter, int sortMode)
	{
		if (!snap.sections.empty())
		{
			PadLayout::BeginList("###gw2igh_wallet_list", 80.f);
			int shown = 0;
			const bool wantWallet = (locFilter == 0 || locFilter == 1);
			if (wantWallet && DrawWalletCurrencies(snap, filter))
				++shown;
			if (locFilter != 1)
			{
				for (const SlotSection& s : snap.sections)
				{
					if (locFilter > 0 && s.kind != static_cast<LocKind>(locFilter - 1))
						continue;
					if (s.kind == Loc_Wallet)
						continue;
					bool any = !(filter && filter[0]);
					if (filter && filter[0])
					{
						for (const SlotCell& c : s.slots)
						{
							if (SlotMatches(c, filter))
							{
								any = true;
								break;
							}
						}
					}
					if (!any)
						continue;
					if (s.kind == Loc_Character && s.filled <= 0)
						continue;
					char head[160];
					char foldId[96];
					if (s.kind == Loc_Materials && s.capacity > 0)
						std::snprintf(head, sizeof(head), "%s    %d of %d Types Collected",
							s.title.c_str(), s.filled, s.capacity);
					else if (s.kind == Loc_Bank)
						std::snprintf(head, sizeof(head), "%s", s.title.c_str());
					else if (s.kind == Loc_Character)
					{
						std::string bag;
						if (s.itemId > 0)
							bag = LookupName(s.itemId, s.itemId, false);
						if (bag.rfind("Item #", 0) == 0 || bag.rfind("Currency #", 0) == 0)
							bag.clear();
						if (!bag.empty())
							std::snprintf(head, sizeof(head), "%s — %s  ·  %d",
								s.title.c_str(), bag.c_str(), s.filled);
						else
							std::snprintf(head, sizeof(head), "%s — Bag %d  ·  %d",
								s.title.c_str(), s.bagN > 0 ? s.bagN : 1, s.filled);
					}
					else
						std::snprintf(head, sizeof(head), "%s  ·  %d", s.title.c_str(), s.filled);
					std::snprintf(foldId, sizeof(foldId), "%s/%d/%d",
						s.title.c_str(), s.bagN, s.itemId);
					const bool startOpen = s.kind == Loc_Bank || s.kind == Loc_Shared ||
						(locFilter > 0 && locFilter != 1);
					if (!BeginFold(foldId, head, startOpen))
					{
						++shown;
						continue;
					}
					DrawSlotGrid(s.slots, filter);
					EndFold();
					++shown;
				}
			}
			if (shown == 0 && locFilter == 2)
			{
				std::vector<StashRow> rows;
				CollectStashRows(snap, filter, locFilter, rows);
				SortStashRows(rows, sortMode);
				if (!rows.empty())
				{
					ImGui::TextColored(HelperTheme::Muted, "%d stacks",
						static_cast<int>(rows.size()));
					for (const StashRow& row : rows)
						DrawStashRow(row, locFilter, false);
					shown = static_cast<int>(rows.size());
				}
			}
			if (shown == 0 && !gBusy)
				DrawEmptyStash(snap, filter, locFilter);
			PadLayout::EndList();
			return;
		}

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
