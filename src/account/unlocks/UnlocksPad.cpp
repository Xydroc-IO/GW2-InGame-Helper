#include "UnlocksPad.h"

#include "Globals.h"
#include "Gw2Icons.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "UnlocksData.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
	std::string LowerCopy(std::string s)
	{
		for (char& c : s)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return s;
	}

	bool Contains(const std::string& hay, const char* needle)
	{
		return hay.find(needle) != std::string::npos;
	}

	bool DrawUnlockIcon(UnlocksData::Kind kind, const UnlocksData::Row& r, float sz)
	{
		if (kind == UnlocksData::Kind::Dyes)
		{
			if (!r.hasRgb)
				return false;
			char id[40];
			std::snprintf(id, sizeof(id), "###gw2igh_dye_%d", r.id);
			const ImVec4 col(
				static_cast<float>((r.rgb >> 16) & 255) / 255.f,
				static_cast<float>((r.rgb >> 8) & 255) / 255.f,
				static_cast<float>(r.rgb & 255) / 255.f,
				1.f);
			ImGui::ColorButton(id, col,
				ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoTooltip |
				ImGuiColorEditFlags_NoDragDrop,
				ImVec2(sz, sz));
			return true;
		}
		if (r.iconUrl.empty())
			return false;
		return Gw2Icons::ImageUrl(r.iconUrl.c_str(), sz);
	}

	void Tip(const UnlocksData::Row& r)
	{
		if (!ImGui::IsItemHovered())
			return;
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(r.name.c_str());
		ImGui::TextDisabled("#%d", r.id);
		ImGui::EndTooltip();
	}

	bool BeginFold(const char* id, const char* label, bool startOpen)
	{
		char treeId[192];
		std::snprintf(treeId, sizeof(treeId), "%s###gw2igh_unlock_fold_%s",
			label ? label : "(group)", id ? id : "x");
		ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::GoldMuted);
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
		if (startOpen)
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		const bool open = ImGui::TreeNodeEx(treeId, flags);
		ImGui::PopStyleColor();
		return open;
	}

	void EndFold()
	{
		ImGui::TreePop();
	}

	/* --- classifiers --- */

	struct SkinKey
	{
		const char* type = "Other";
		const char* slot = "Other";
	};

	SkinKey ClassifySkin(const std::string& name)
	{
		const std::string h = LowerCopy(name);
		SkinKey k;
		if (Contains(h, "logging") || Contains(h, "harvesting") || Contains(h, "mining pick")
			|| Contains(h, "sickle") || Contains(h, "mining"))
		{
			k.type = "Gathering";
			if (Contains(h, "log")) k.slot = "Logging";
			else if (Contains(h, "mine") || Contains(h, "pick")) k.slot = "Mining";
			else k.slot = "Harvesting";
			return k;
		}
		if (Contains(h, "backpack") || Contains(h, "backpiece") || Contains(h, "back piece")
			|| Contains(h, "cape"))
		{
			k.type = "Back";
			k.slot = "Back";
			return k;
		}

		static const char* const kWeap[][2] = {
			{ "greatsword", "Greatsword" }, { "longbow", "Longbow" },
			{ "short bow", "Short Bow" }, { "shortbow", "Short Bow" },
			{ "harpoon", "Spear" }, { "speargun", "Speargun" },
			{ "warhorn", "Warhorn" }, { "war horn", "Warhorn" },
			{ "scepter", "Scepter" }, { "trident", "Trident" },
			{ "spear", "Spear" }, { "staff", "Staff" },
			{ "hammer", "Hammer" }, { "rifle", "Rifle" },
			{ "pistol", "Pistol" }, { "dagger", "Dagger" },
			{ "focus", "Focus" }, { "shield", "Shield" },
			{ "torch", "Torch" }, { "mace", "Mace" },
			{ "sword", "Sword" }, { "axe", "Axe" },
		};
		for (const auto& w : kWeap)
		{
			if (Contains(h, w[0]))
			{
				k.type = "Weapon";
				k.slot = w[1];
				return k;
			}
		}

		static const char* const kSlot[][2] = {
			{ "shoulders", "Shoulders" }, { "pauldrons", "Shoulders" },
			{ "gauntlets", "Hands" }, { "gloves", "Hands" }, { "vambraces", "Hands" },
			{ "bracers", "Hands" }, { "wraps", "Hands" }, { "grips", "Hands" },
			{ "leggings", "Legs" }, { "tassets", "Legs" }, { "chausses", "Legs" },
			{ "breeches", "Legs" }, { "greaves", "Legs" },
			{ "sabatons", "Feet" }, { "boots", "Feet" }, { "treads", "Feet" },
			{ "shoes", "Feet" },
			{ "headgear", "Head" }, { "helmet", "Head" }, { "helm", "Head" },
			{ "circlet", "Head" }, { "crown", "Head" }, { "coif", "Head" },
			{ "cowl", "Head" }, { "hood", "Head" }, { "mask", "Head" },
			{ "diadem", "Head" }, { "tiara", "Head" },
			{ "hauberk", "Chest" }, { "cuirass", "Chest" }, { "vestments", "Chest" },
			{ "doublet", "Chest" }, { "jerkin", "Chest" }, { "tunic", "Chest" },
			{ "jacket", "Chest" }, { "raiment", "Chest" }, { "coat", "Chest" },
			{ "garb", "Chest" }, { "apparel", "Chest" },
		};
		for (const auto& s : kSlot)
		{
			if (Contains(h, s[0]))
			{
				k.type = "Armor";
				k.slot = s[1];
				return k;
			}
		}
		return k;
	}

	const char* DyeHue(const UnlocksData::Row& r)
	{
		if (!r.hasRgb)
			return "Other";
		const float R = static_cast<float>((r.rgb >> 16) & 255) / 255.f;
		const float G = static_cast<float>((r.rgb >> 8) & 255) / 255.f;
		const float B = static_cast<float>(r.rgb & 255) / 255.f;
		const float mx = (std::max)(R, (std::max)(G, B));
		const float mn = (std::min)(R, (std::min)(G, B));
		const float d = mx - mn;
		const float s = (mx <= 1e-5f) ? 0.f : d / mx;
		if (s < 0.14f)
		{
			if (mx < 0.16f) return "Black";
			if (mx > 0.88f) return "White";
			return "Gray";
		}
		float h = 0.f;
		if (d > 1e-5f)
		{
			if (mx == R) h = (G - B) / d;
			else if (mx == G) h = (B - R) / d + 2.f;
			else h = (R - G) / d + 4.f;
			h *= 60.f;
			if (h < 0.f) h += 360.f;
		}
		if (h >= 18.f && h < 48.f && s < 0.72f && mx < 0.62f)
			return "Brown";
		if (h < 14.f || h >= 345.f) return "Red";
		if (h < 40.f) return "Orange";
		if (h < 70.f) return "Yellow";
		if (h < 165.f) return "Green";
		if (h < 200.f) return "Cyan";
		if (h < 255.f) return "Blue";
		if (h < 290.f) return "Purple";
		return "Pink";
	}

	char LetterOf(const UnlocksData::Row& r)
	{
		unsigned char c = 0;
		for (char ch : r.name)
		{
			if (static_cast<unsigned char>(ch) > 32)
			{
				c = static_cast<unsigned char>(ch);
				break;
			}
		}
		if (c >= 'a' && c <= 'z') c = static_cast<unsigned char>(c - 32);
		if (c >= 'A' && c <= 'Z') return static_cast<char>(c);
		return '#';
	}

	/* --- layout --- */

	bool WrapCell(float cellW, bool first)
	{
		if (first)
			return true;
		const ImGuiStyle& st = ImGui::GetStyle();
		const float last = ImGui::GetItemRectMax().x;
		const float next = last + st.ItemSpacing.x + cellW;
		const float edge = ImGui::GetWindowPos().x + ImGui::GetContentRegionMax().x - 8.f;
		if (next < edge)
		{
			ImGui::SameLine(0.f, st.ItemSpacing.x);
			return true;
		}
		return true;
	}

	void DrawDyeGrid(const std::vector<const UnlocksData::Row*>& rows)
	{
		const float cell = 22.f;
		bool first = true;
		for (const UnlocksData::Row* r : rows)
		{
			WrapCell(cell, first);
			first = false;
			ImGui::PushID(r->id);
			if (!DrawUnlockIcon(UnlocksData::Kind::Dyes, *r, cell))
				ImGui::Dummy(ImVec2(cell, cell));
			Tip(*r);
			ImGui::PopID();
		}
	}

	void DrawIconGrid(UnlocksData::Kind kind, const std::vector<const UnlocksData::Row*>& rows)
	{
		const float cell = 36.f;
		bool first = true;
		for (const UnlocksData::Row* r : rows)
		{
			WrapCell(cell, first);
			first = false;
			ImGui::PushID(r->id);
			if (!DrawUnlockIcon(kind, *r, cell))
				ImGui::Dummy(ImVec2(cell, cell));
			Tip(*r);
			ImGui::PopID();
		}
	}

	void DrawNameWrap(UnlocksData::Kind kind, const std::vector<const UnlocksData::Row*>& rows)
	{
		const float colW = 210.f;
		bool first = true;
		for (const UnlocksData::Row* r : rows)
		{
			WrapCell(colW, first);
			first = false;
			ImGui::PushID(r->id);
			ImGui::BeginGroup();
			if (DrawUnlockIcon(kind, *r, 20.f))
				ImGui::SameLine(0.f, 6.f);
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + colW - 28.f);
			ImGui::TextUnformatted(r->name.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndGroup();
			Tip(*r);
			ImGui::PopID();
		}
	}

	using GroupMap = std::unordered_map<std::string, std::vector<const UnlocksData::Row*>>;

	template<typename Fn>
	void DrawGroups(const char* const* order, int n, const GroupMap& map,
		bool searching, Fn draw)
	{
		for (int i = 0; i < n; ++i)
		{
			const char* key = order[i];
			auto it = map.find(key);
			if (it == map.end() || it->second.empty())
				continue;
			char head[80];
			std::snprintf(head, sizeof(head), "%s  %d", key, static_cast<int>(it->second.size()));
			const bool open = searching || it->second.size() <= 24;
			if (!BeginFold(key, head, open))
				continue;
			draw(it->second);
			EndFold();
		}
	}

	int FillLetters(char letters[][2], const char* order[], int maxN)
	{
		int n = 0;
		for (char c = 'A'; c <= 'Z' && n < maxN; ++c)
		{
			letters[n][0] = c;
			letters[n][1] = 0;
			order[n] = letters[n];
			++n;
		}
		if (n < maxN)
		{
			letters[n][0] = '#';
			letters[n][1] = 0;
			order[n] = letters[n];
			++n;
		}
		return n;
	}

	std::vector<const UnlocksData::Row*> Flatten(const GroupMap& map)
	{
		std::vector<const UnlocksData::Row*> all;
		for (const auto& kv : map)
			all.insert(all.end(), kv.second.begin(), kv.second.end());
		return all;
	}

	template<typename Fn>
	void DrawTypeFold(const char* type, bool foldOuter, bool searching, size_t n, Fn body)
	{
		char head[80];
		std::snprintf(head, sizeof(head), "%s  %d", type, static_cast<int>(n));
		if (foldOuter)
		{
			if (!BeginFold(type, head, searching || n <= 40))
				return;
			body();
			EndFold();
			return;
		}
		ImGui::TextColored(HelperTheme::GoldMuted, "%s", head);
		body();
	}
}

void UnlocksPad::RenderContents()
{
	UnlocksData::Tick();

	PadNav::Blurb(
		"Wardrobe unlocks from the official API. Grouped grids — hover a tile for the name. "
		"Needs an API key with the unlocks scope.");
	ImGui::Spacing();

	if (!G::Gw2ApiKey[0])
	{
		ImGui::TextColored(HelperTheme::Warn, "No API key - add one in Settings (helper side rail).");
		return;
	}

	if (PadNav::RefreshButton("###gw2igh_unlocks_ref"))
		UnlocksData::EnsureAll(true);
	if (UnlocksData::BusyAny())
	{
		ImGui::SameLine();
		PadNav::StatusBusy("Loading...");
	}

	static int sKind = 0;
	static int sSkinType = 0;
	static int sDyeHue = 0;
	static char sFilter[96] = {};
	static const char* kKindTabs[] = {
		"Skins", "Dyes", "Minis", "Finishers", "Outfits",
		"Gliders", "Mail carriers", "Novelties", "Titles"
	};
	static const int kKindIcons[] = {
		static_cast<int>(Gw2Ui::Icon::Hero),
		static_cast<int>(Gw2Ui::Icon::Gem),
		static_cast<int>(Gw2Ui::Icon::Contacts),
		static_cast<int>(Gw2Ui::Icon::Check),
		static_cast<int>(Gw2Ui::Icon::Inventory),
		static_cast<int>(Gw2Ui::Icon::Map),
		static_cast<int>(Gw2Ui::Icon::Mail),
		static_cast<int>(Gw2Ui::Icon::Trade),
		static_cast<int>(Gw2Ui::Icon::Achievements),
	};
	const int prev = sKind;
	sKind = PadNav::DrawSideRail("###gw2igh_unlock_kinds", kKindTabs,
		static_cast<int>(UnlocksData::Kind::Count), sKind, 0.f, kKindIcons);
	const auto kind = static_cast<UnlocksData::Kind>(sKind);
	if (sKind != prev)
	{
		sSkinType = 0;
		sDyeHue = 0;
		UnlocksData::EnsureLoaded(kind, false);
	}
	else if (!UnlocksData::Ready(kind))
		UnlocksData::EnsureLoaded(kind, false);

	ImGui::BeginChild("###gw2igh_unlock_body", ImVec2(0.f, 0.f), true, PadNav::kLockScroll);
	ImGui::TextDisabled("%s - %zu unlocked | %s",
		UnlocksData::KindLabel(kind),
		UnlocksData::Count(kind),
		UnlocksData::Status(kind));

	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputTextWithHint("###gw2igh_unlock_filter", "Filter name or id...",
		sFilter, sizeof(sFilter));

	std::vector<UnlocksData::Row> rows;
	UnlocksData::Search(kind, sFilter, rows);
	const bool searching = sFilter[0] != '\0';

	ImGui::BeginChild("###gw2igh_unlock_list", ImVec2(0.f, 0.f), false);
	if (!UnlocksData::Ready(kind) && UnlocksData::Busy(kind))
		ImGui::TextDisabled("Loading %s...", UnlocksData::KindLabel(kind));
	else if (rows.empty())
		ImGui::TextDisabled(sFilter[0] ? "No matches." : "No unlocks loaded yet.");
	else if (kind == UnlocksData::Kind::Dyes)
	{
		static const char* kHues[] = {
			"All", "Red", "Orange", "Yellow", "Green", "Cyan", "Blue",
			"Purple", "Pink", "Brown", "Gray", "White", "Black"
		};
		constexpr int kHueN = static_cast<int>(sizeof(kHues) / sizeof(kHues[0]));
		PadNav::Meta("Hue");
		ImGui::PushID("###gw2igh_dye_hue");
		for (int i = 0; i < kHueN; ++i)
		{
			ImGui::PushID(i);
			if (PadNav::WrapButton(kHues[i], i == sDyeHue, i == 0))
				sDyeHue = i;
			ImGui::PopID();
		}
		ImGui::PopID();
		ImGui::Spacing();

		GroupMap byHue;
		std::vector<const UnlocksData::Row*> vis;
		vis.reserve(rows.size());
		for (const UnlocksData::Row& r : rows)
		{
			const char* hue = DyeHue(r);
			if (sDyeHue > 0 && std::strcmp(hue, kHues[sDyeHue]) != 0)
				continue;
			vis.push_back(&r);
			byHue[hue].push_back(&r);
		}
		if (vis.empty())
			ImGui::TextDisabled("No dyes in this hue.");
		else if (sDyeHue > 0 || searching)
			DrawDyeGrid(vis);
		else
		{
			DrawGroups(kHues + 1, kHueN - 1, byHue, searching, DrawDyeGrid);
			auto other = byHue.find("Other");
			if (other != byHue.end() && !other->second.empty())
			{
				if (BeginFold("Other", "Other", true))
				{
					DrawDyeGrid(other->second);
					EndFold();
				}
			}
		}
	}
	else if (kind == UnlocksData::Kind::Skins)
	{
		static const char* kTypes[] = { "All", "Armor", "Weapon", "Back", "Gathering", "Other" };
		constexpr int kTypeN = 6;
		PadNav::Meta("Type");
		ImGui::PushID("###gw2igh_skin_type");
		for (int i = 0; i < kTypeN; ++i)
		{
			ImGui::PushID(i);
			if (PadNav::WrapButton(kTypes[i], i == sSkinType, i == 0))
				sSkinType = i;
			ImGui::PopID();
		}
		ImGui::PopID();
		ImGui::Spacing();

		std::unordered_map<std::string, GroupMap> nested;
		size_t shown = 0;
		for (const UnlocksData::Row& r : rows)
		{
			const SkinKey sk = ClassifySkin(r.name);
			if (sSkinType > 0 && std::strcmp(sk.type, kTypes[sSkinType]) != 0)
				continue;
			nested[sk.type][sk.slot].push_back(&r);
			++shown;
		}
		if (shown == 0)
			ImGui::TextDisabled("No skins in this type.");
		else
		{
			static const char* kArmorSlots[] = {
				"Head", "Shoulders", "Chest", "Hands", "Legs", "Feet", "Other"
			};
			static const char* kWeapSlots[] = {
				"Axe", "Dagger", "Mace", "Pistol", "Scepter", "Sword",
				"Focus", "Shield", "Torch", "Warhorn",
				"Greatsword", "Hammer", "Longbow", "Rifle", "Short Bow", "Staff",
				"Spear", "Speargun", "Trident", "Other"
			};
			static const char* kGathSlots[] = { "Logging", "Mining", "Harvesting", "Other" };
			const auto drawNames = [kind](const std::vector<const UnlocksData::Row*>& g) {
				DrawNameWrap(kind, g);
			};
			const bool foldOuter = sSkinType == 0;
			auto runType = [&](const char* type, const char* const* slots, int nSlots, bool flatten) {
				auto nit = nested.find(type);
				if (nit == nested.end() || nit->second.empty())
					return;
				size_t n = 0;
				for (const auto& kv : nit->second)
					n += kv.second.size();
				DrawTypeFold(type, foldOuter, searching, n, [&]() {
					if (flatten)
						DrawNameWrap(kind, Flatten(nit->second));
					else
						DrawGroups(slots, nSlots, nit->second, searching, drawNames);
				});
			};
			runType("Armor", kArmorSlots, 7, false);
			runType("Weapon", kWeapSlots, 20, false);
			runType("Back", nullptr, 0, true);
			runType("Gathering", kGathSlots, 4, false);
			runType("Other", nullptr, 0, true);
		}
	}
	else if (kind == UnlocksData::Kind::Titles)
	{
		GroupMap byLet;
		char letters[28][2]{};
		const char* order[27]{};
		for (const UnlocksData::Row& r : rows)
			byLet[std::string(1, LetterOf(r))].push_back(&r);
		const int nLet = FillLetters(letters, order, 27);
		DrawGroups(order, nLet, byLet, searching, [kind](const std::vector<const UnlocksData::Row*>& g) {
			DrawNameWrap(kind, g);
		});
	}
	else
	{
		/* Minis / finishers / outfits / gliders / mail / novelties — icon tiles. */
		if (rows.size() <= 24)
		{
			std::vector<const UnlocksData::Row*> vis;
			vis.reserve(rows.size());
			for (const UnlocksData::Row& r : rows)
				vis.push_back(&r);
			DrawIconGrid(kind, vis);
		}
		else
		{
			GroupMap byLet;
			char letters[28][2]{};
			const char* order[27]{};
			for (const UnlocksData::Row& r : rows)
				byLet[std::string(1, LetterOf(r))].push_back(&r);
			const int nLet = FillLetters(letters, order, 27);
			DrawGroups(order, nLet, byLet, searching, [kind](const std::vector<const UnlocksData::Row*>& g) {
				DrawIconGrid(kind, g);
			});
		}
	}
	ImGui::EndChild();
	ImGui::EndChild();
}
