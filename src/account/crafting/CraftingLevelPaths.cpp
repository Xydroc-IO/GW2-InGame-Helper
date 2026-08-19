#include "CraftingData.h"

#include "CraftingShared.h"

#include "BrowserTabs.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>

namespace CraftingDetail
{
	/* Curated discipline leveling targets — Plan resolves via API/wiki. */
	namespace
	{
		struct PathStep
		{
			int itemId;
			const char* name;
			const char* band;
		};

		struct DiscPath
		{
			const char* discipline; /* UI label + API preferDiscipline where applicable */
			const char* craftsUrl;  /* gw2crafts deep-link (homepage if unknown) */
			const PathStep* steps;
			int stepCount;
		};

		/* Existing Cooking handoffs — keep ids/names as shipped. */
		constexpr PathStep kCooking[] = {
			{ 12134, "Bowl of Basic Poultry Soup", "0–50" },
			{ 12152, "Bowl of Simple Chili", "50–100" },
			{ 12245, "Bowl of Chickpea Soup", "100–150" },
			{ 12453, "Plate of Meaty Asparagus", "150–200" },
			{ 12391, "Bowl of Spiced Vegetable Soup", "200–250" },
			{ 12488, "Bowl of Curry Squash Soup", "250–300" },
			{ 12516, "Bowl of Lemongrass Poultry Soup", "300–350" },
			{ 12633, "Bowl of Spicy Meat Chili", "350–400" },
			{ 36075, "Bowl of Truffle Ravioli", "400–450" },
			{ 38314, "Bowl of Orrian Steak Frites", "450–500" },
		};

		constexpr PathStep kArmorsmith[] = {
			{ 13102, "Bronze Helmet Casing", "0–75" },
			{ 13119, "Iron Casque Casing", "75–150" },
			{ 13131, "Steel Splint Helmet Casing", "150–225" },
			{ 13143, "Darksteel Helmet Casing", "225–300" },
			{ 13155, "Mithril Helmet Casing", "300–400" },
			{ 13167, "Orichalcum Helmet Casing", "400" },
		};

		constexpr PathStep kWeaponsmith[] = {
			{ 12848, "Bronze Axe Blade", "0–75" },
			{ 12850, "Iron Axe Blade", "75–150" },
			{ 12853, "Steel Axe Blade", "150–225" },
			{ 12849, "Darksteel Axe Blade", "225–300" },
			{ 12851, "Mithril Axe Blade", "300–400" },
			{ 12852, "Orichalcum Axe Blade", "400" },
		};

		constexpr PathStep kHuntsman[] = {
			{ 12945, "Green Short-Bow Stave", "0–75" },
			{ 12948, "Soft Short-Bow Stave", "75–150" },
			{ 12949, "Seasoned Short-Bow Stave", "150–225" },
			{ 12946, "Hard Short-Bow Stave", "225–300" },
			{ 12944, "Elder Short-Bow Stave", "300–400" },
			{ 12947, "Ancient Short-Bow Stave", "400" },
		};

		constexpr PathStep kArtificer[] = {
			{ 12984, "Green Focus Casing", "0–75" },
			{ 12987, "Soft Focus Casing", "75–150" },
			{ 12986, "Seasoned Focus Casing", "150–225" },
			{ 12985, "Hard Focus Casing", "225–300" },
			{ 12983, "Elder Focus Casing", "300–400" },
			{ 12982, "Ancient Focus Casing", "400" },
		};

		constexpr PathStep kTailor[] = {
			{ 13021, "Jute Sandal Upper", "0–75" },
			{ 13034, "Wool Footwear Upper", "75–150" },
			{ 13046, "Cotton Shoe Upper", "150–225" },
			{ 13178, "Linen Shoe Upper", "225–300" },
			{ 13190, "Silk Shoe Upper", "300–400" },
			{ 13202, "Gossamer Shoe Upper", "400" },
		};

		constexpr PathStep kLeatherworker[] = {
			{ 13091, "Rawhide Boot Upper", "0–75" },
			{ 13078, "Thin Boot Upper", "75–150" },
			{ 13065, "Coarse Boot Upper", "150–225" },
			{ 13209, "Rugged Boot Upper", "225–300" },
			{ 13221, "Thick Boot Upper", "300–400" },
			{ 13233, "Hardened Boot Upper", "400" },
		};

		constexpr PathStep kJeweler[] = {
			{ 12824, "Copper Band", "0–75" },
			{ 12829, "Silver Band", "75–150" },
			{ 12825, "Gold Band", "150–225" },
			{ 12828, "Platinum Band", "225–300" },
			{ 12826, "Mithril Band", "300–400" },
			{ 12827, "Orichalcum Band", "400" },
		};

		constexpr PathStep kScribe[] = {
			{ 76518, "Simple Scribing Kit", "0–75" },
			{ 72955, "Basic Scribing Kit", "75–150" },
			{ 70765, "Fine Scribing Kit", "150–225" },
			{ 72925, "Journeyman's Scribing Kit", "225–300" },
			{ 71136, "Sheet of Quality Paper", "300–400" },
			{ 71148, "Sheet of Premium Paper", "400" },
		};

		/* Canonical pages on https://gw2crafts.net/ (no www; huntsman/tailor not hunting/tailoring). */
		constexpr DiscPath kPaths[] = {
			{ "Cooking", "https://gw2crafts.net/cooking.html", kCooking,
				(int)(sizeof(kCooking) / sizeof(kCooking[0])) },
			{ "Armorsmith", "https://gw2crafts.net/armorcraft.html", kArmorsmith,
				(int)(sizeof(kArmorsmith) / sizeof(kArmorsmith[0])) },
			{ "Weaponsmith", "https://gw2crafts.net/weaponcraft.html", kWeaponsmith,
				(int)(sizeof(kWeaponsmith) / sizeof(kWeaponsmith[0])) },
			{ "Huntsman", "https://gw2crafts.net/huntsman.html", kHuntsman,
				(int)(sizeof(kHuntsman) / sizeof(kHuntsman[0])) },
			{ "Artificer", "https://gw2crafts.net/artificing.html", kArtificer,
				(int)(sizeof(kArtificer) / sizeof(kArtificer[0])) },
			{ "Tailor", "https://gw2crafts.net/tailor.html", kTailor,
				(int)(sizeof(kTailor) / sizeof(kTailor[0])) },
			{ "Leatherworker", "https://gw2crafts.net/leatherworking.html", kLeatherworker,
				(int)(sizeof(kLeatherworker) / sizeof(kLeatherworker[0])) },
			{ "Jeweler", "https://gw2crafts.net/jewelcraft.html", kJeweler,
				(int)(sizeof(kJeweler) / sizeof(kJeweler[0])) },
			{ "Scribe", "https://gw2crafts.net/scribe.html", kScribe,
				(int)(sizeof(kScribe) / sizeof(kScribe[0])) },
		};

		constexpr int kPathCount = (int)(sizeof(kPaths) / sizeof(kPaths[0]));

		void OpenCraftsUrl(const char* url)
		{
			if (!url || !url[0])
				url = "https://gw2crafts.net/";
			G::ShowWiki = true;
			Settings::SetDirty();
			/* Site id gw2crafts (catalog Tools) — not browse hub. */
			if (BrowserTabs::OpenNewUrl("gw2crafts", url) < 0)
				BrowserTabs::OpenUrlInActive("gw2crafts", url);
		}

		void DrawPathRows(const DiscPath& path)
		{
			for (int i = 0; i < path.stepCount; ++i)
			{
				const PathStep& s = path.steps[i];
				ImGui::PushID(s.itemId);
				ImGui::TextColored(HelperTheme::Muted, "%s", s.band);
				ImGui::SameLine();
				ImGui::TextUnformatted(s.name);
				ImGui::SameLine();
				if (ImGui::SmallButton("Plan"))
				{
					std::snprintf(gQuery, sizeof(gQuery), "%d", s.itemId);
					StartPlan();
				}
				ImGui::PopID();
			}
		}
	}

	void DrawLevelingPaths()
	{
		PadNav::SectionTitle("Leveling paths");
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Curated craft targets by discipline — Plan opens the recipe tree (read-only).");
		PadNav::PopWrap();

		static int sDisc = 0;
		if (sDisc < 0 || sDisc >= kPathCount)
			sDisc = 0;

		/* In-pad chips — BeginCombo popups sit outside the pad and GW2/Nexus
		   often eats the click before Selectable fires. */
		{
			const ImGuiStyle& st = ImGui::GetStyle();
			for (int i = 0; i < kPathCount; ++i)
			{
				const char* name = kPaths[i].discipline;
				const float btnW = ImGui::CalcTextSize(name).x + st.FramePadding.x * 2.f;
				if (i > 0)
					PadNav::WrapSameLine(btnW);

				const bool sel = (i == sDisc);
				if (sel)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, HelperTheme::TabActive);
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HelperTheme::Header);
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, HelperTheme::Header);
					ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::GoldBright);
				}
				ImGui::PushID(i);
				if (ImGui::Button(name))
					sDisc = i;
				ImGui::PopID();
				if (sel)
					ImGui::PopStyleColor(4);
			}
		}

		if (ImGui::SmallButton("GW2 Crafts###gw2igh_lvl_crafts"))
			OpenCraftsUrl(kPaths[sDisc].craftsUrl);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Open gw2crafts.net for %s in a Browse tab",
				kPaths[sDisc].discipline);

		ImGui::Spacing();
		ImGui::TextColored(HelperTheme::GoldDim, "%s", kPaths[sDisc].discipline);
		DrawPathRows(kPaths[sDisc]);
	}

	void DrawCookingPath()
	{
		DrawLevelingPaths();
	}
}
