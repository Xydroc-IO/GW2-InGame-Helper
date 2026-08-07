#include "CraftingData.h"

#include "CraftingShared.h"

#include "EconomyShared.h"
#include "HelperTheme.h"
#include "PadNav.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace CraftingDetail
{
	static void DrawNode(const IngNode& n)
	{
		ImGui::PushID(n.itemId + n.depth * 1000003);
		const char* name = n.name.empty() ? "..." : n.name.c_str();
		const int miss = (std::max)(0, n.need - n.have);
		const bool ok = miss <= 0;
		char label[256];
		if (n.crafted && !n.kids.empty())
			std::snprintf(label, sizeof(label), "[craft] %s  %d / %d", name, n.have, n.need);
		else
			std::snprintf(label, sizeof(label), "%s  %d / %d", name, n.have, n.need);
		if (!n.kids.empty())
		{
			if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const IngNode& k : n.kids)
					DrawNode(k);
				ImGui::TreePop();
			}
		}
		else
		{
			if (ok)
				ImGui::TextColored(HelperTheme::Ok, "%s", label);
			else
				ImGui::TextColored(HelperTheme::Warn, "%s", label);
			if (miss > 0 && n.buyUnit >= 0)
			{
				ImGui::SameLine();
				ImGui::TextColored(HelperTheme::Muted,
					"buy %s", FormatCoins(n.buyUnit * miss).c_str());
			}
			else if (miss > 0)
			{
				ImGui::SameLine();
				ImGui::TextColored(HelperTheme::Warn, "no TP");
			}
		}
		ImGui::PopID();
	}

	void DrawOptsBar()
	{
		bool dirty = false;
		if (ImGui::Checkbox("Use owned materials###gw2igh_opt_own", &gOpts.useOwnMaterials))
			dirty = true;
		ImGui::SameLine();
		if (ImGui::Checkbox("Craft sub-components###gw2igh_opt_sub", &gOpts.craftSubComponents))
			dirty = true;
		ImGui::SameLine();
		if (ImGui::Checkbox("Group by item###gw2igh_opt_grp", &gOpts.groupByItem))
			dirty = true;
		if (dirty)
		{
			SaveCraftOpts();
			if (gPlan.ok)
				StartPlan();
		}
	}

	static long long ListNet(const Plan& plan)
	{
		if (plan.tpListUnit < 0) return -1;
		const int want = (std::max)(1, plan.wantQty > 0 ? plan.wantQty : 1);
		return (plan.tpListUnit * want * 85) / 100;
	}

	static void DrawFinancialOne(const Plan& plan)
	{
		PadNav::SectionTitle("Financial breakdown");
		const int want = (std::max)(1, plan.wantQty > 0 ? plan.wantQty : 1);
		ImGui::TextColored(HelperTheme::Muted, "Want x%d (crafts yield %d ea)",
			want, plan.outputCount);
		ImGui::TextColored(HelperTheme::Ink, "Craft cost:  %s",
			FormatCoins(plan.buyTotal).c_str());
		if (plan.tpBuyOutright >= 0)
		{
			ImGui::TextColored(HelperTheme::Ink, "Buy outright: %s",
				FormatCoins(plan.tpBuyOutright).c_str());
			const long long save = plan.tpBuyOutright - plan.buyTotal;
			if (save > 0)
				ImGui::TextColored(HelperTheme::Ok, "Savings vs buy: %s",
					FormatCoins(save).c_str());
			else if (save < 0)
				ImGui::TextColored(HelperTheme::Warn, "Buy cheaper by: %s",
					FormatCoins(-save).c_str());
		}
		else
			ImGui::TextColored(HelperTheme::Muted, "Buy outright: N/A");

		const long long listNet = ListNet(plan);
		if (listNet >= 0)
		{
			ImGui::TextColored(HelperTheme::Muted, "Sell listed (net ~85%%): %s",
				FormatCoins(listNet).c_str());
			const long long profit = listNet - plan.buyTotal;
			if (profit >= 0)
				ImGui::TextColored(HelperTheme::Ok, "Profit (craft→list): %s",
					FormatCoins(profit).c_str());
			else
				ImGui::TextColored(HelperTheme::Warn, "Loss (craft→list): %s",
					FormatCoins(-profit).c_str());
			if (plan.buyTotal > 0)
				ImGui::TextColored(HelperTheme::Muted, "ROI: %lld%%",
					(profit * 100) / plan.buyTotal);
		}
		if (plan.tpInstantUnit >= 0)
		{
			const long long inst = (plan.tpInstantUnit * want * 85) / 100;
			ImGui::TextColored(HelperTheme::Muted, "Instant sell (net ~85%%): %s",
				FormatCoins(inst).c_str());
		}
		if (plan.noTpMissing > 0)
			ImGui::TextColored(HelperTheme::Warn,
				"Some mats have no TP price — craft cost is a lower bound.");
	}

	static void DrawShopRows(const std::vector<ShopRow>& rows, bool allowCartGot)
	{
		if (rows.empty())
		{
			ImGui::TextColored(HelperTheme::Ok, "All leaf mats on hand (or nothing to buy).");
			return;
		}
		int checked = 0;
		long long stillNeed = 0;
		for (const ShopRow& s : rows)
		{
			const bool got = allowCartGot && CartIsGot(s.itemId, nullptr);
			if (got) ++checked;
			else if (s.priced && s.total >= 0)
				stillNeed += s.total;

			ImGui::PushID(s.itemId);
			if (allowCartGot)
			{
				bool mark = got;
				char label[256];
				std::snprintf(label, sizeof(label), "%s  x%d",
					s.name.empty() ? "Item" : s.name.c_str(), s.qty);
				if (ImGui::Checkbox(label, &mark))
					CartSetGot(s.itemId, mark, nullptr);
			}
			else
				ImGui::Text("%s  x%d", s.name.empty() ? "Item" : s.name.c_str(), s.qty);

			if (got)
			{
				ImGui::SameLine();
				ImGui::TextColored(HelperTheme::Muted, "got it");
			}
			else if (s.priced)
			{
				ImGui::SameLine();
				ImGui::TextColored(HelperTheme::Muted, "%s ea · %s",
					FormatCoins(s.unitSell).c_str(), FormatCoins(s.total).c_str());
			}
			else
			{
				ImGui::SameLine();
				ImGui::TextColored(HelperTheme::Warn, "no TP");
			}
			ImGui::PopID();
		}
		ImGui::TextColored(HelperTheme::Muted, "%d / %d checked",
			checked, static_cast<int>(rows.size()));
		if (stillNeed > 0)
			ImGui::TextColored(HelperTheme::Ink, "Still need: %s",
				FormatCoins(stillNeed).c_str());
		if (ImGui::SmallButton("Add missing to Economy cart###gw2igh_craft_ecart"))
		{
			for (const ShopRow& s : rows)
			{
				if (allowCartGot && CartIsGot(s.itemId, nullptr)) continue;
				if (s.qty <= 0) continue;
				EconomyDetail::AddToCart(s.itemId,
					s.name.empty() ? "Item" : s.name.c_str(), s.qty);
			}
		}
		if (allowCartGot)
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Clear checks###gw2igh_craft_clrgot"))
			{
				for (const ShopRow& s : rows)
					CartSetGot(s.itemId, false, nullptr);
			}
		}
	}

	static void DrawStepsList(const std::vector<StepRow>& steps)
	{
		if (steps.empty())
		{
			ImGui::TextColored(HelperTheme::Muted, "No craft steps (buy-only leaves).");
			return;
		}
		int n = 1;
		for (const StepRow& s : steps)
		{
			ImGui::PushID(s.outId + s.depth * 9973 + n);
			const char* nm = s.name.empty() ? "Item" : s.name.c_str();
			ImGui::TextColored(HelperTheme::GoldBright, "%d. Craft %dx  %s",
				n, s.crafts, nm);
			ImGui::SameLine();
			ImGui::TextColored(HelperTheme::Muted, "[%s]",
				s.disc.empty() ? "Craft" : s.disc.c_str());
			for (const RecipeIng& ri : s.ings)
			{
				ImGui::BulletText("%s  x%d",
					ri.name.empty() ? "Item" : ri.name.c_str(), ri.count);
			}
			ImGui::PopID();
			++n;
		}
	}

	static void DrawKnownBadge(const Plan& plan)
	{
		const char* prefer = SelectedKnownChar();
		const int known = RecipeKnownState(plan.recipeId, prefer && prefer[0] ? prefer : nullptr);
		if (known == 1)
		{
			auto who = CharsKnowing(plan.recipeId);
			ImGui::TextColored(HelperTheme::Ok, "Recipe known");
			if (!who.empty())
			{
				ImGui::SameLine();
				ImGui::TextColored(HelperTheme::Muted, "by %s%s",
					who[0].c_str(), who.size() > 1 ? " +…" : "");
			}
		}
		else if (known == 0)
			ImGui::TextColored(HelperTheme::Warn, "Recipe not learned");
		else if (known == -2)
			ImGui::TextColored(HelperTheme::Muted, "No station recipe id (forge / curated)");
	}

	void DrawPlanResults(const Plan& plan, bool allowCartGot)
	{
		ImGui::TextColored(HelperTheme::GoldBright, "%s",
			plan.outputName.empty() ? "Output" : plan.outputName.c_str());
		ImGui::TextColored(HelperTheme::Muted,
			"#%d | %s | want x%d | yield %d",
			plan.outputId,
			plan.recipeSource.empty() ? "recipe" : plan.recipeSource.c_str(),
			(std::max)(1, plan.wantQty), plan.outputCount);
		DrawKnownBadge(plan);
		ImGui::Spacing();
		DrawFinancialOne(plan);
		ImGui::Spacing();
		PadNav::SectionTitle("Shopping list");
		DrawShopRows(plan.shopping, allowCartGot);
		ImGui::Spacing();
		PadNav::SectionTitle("Crafting steps");
		DrawStepsList(plan.steps);
		ImGui::Spacing();
		PadNav::Meta("Recipe tree");
		for (const IngNode& k : plan.root.kids)
			DrawNode(k);
	}

	void DrawAggregatedResults(const std::vector<Plan>& plans, bool allowCartGot)
	{
		long long craftCost = 0, tpCost = 0, listNet = 0;
		bool anyTp = false, anyList = false;
		int noTp = 0;
		std::unordered_map<int, ShopRow> shopAgg;
		std::vector<StepRow> flatSteps;

		PadNav::SectionTitle("Project results");
		for (const Plan& p : plans)
		{
			if (!p.ok)
			{
				ImGui::TextColored(HelperTheme::Warn, "%s — %s",
					p.outputName.empty() ? "Item" : p.outputName.c_str(),
					p.status.c_str());
				continue;
			}
			craftCost += p.buyTotal;
			if (p.tpBuyOutright >= 0) { tpCost += p.tpBuyOutright; anyTp = true; }
			const long long ln = ListNet(p);
			if (ln >= 0) { listNet += ln; anyList = true; }
			noTp += p.noTpMissing;
			for (const ShopRow& s : p.shopping)
			{
				ShopRow& a = shopAgg[s.itemId];
				a.itemId = s.itemId;
				a.name = s.name;
				a.qty += s.qty;
				a.unitSell = s.unitSell;
				a.priced = a.priced || s.priced;
				if (s.total >= 0)
					a.total = (a.total < 0 ? 0 : a.total) + s.total;
			}
			for (const StepRow& st : p.steps)
				flatSteps.push_back(st);
		}

		ImGui::TextColored(HelperTheme::Ink, "Craft cost:  %s", FormatCoins(craftCost).c_str());
		if (anyTp)
		{
			ImGui::TextColored(HelperTheme::Ink, "Buy outright: %s", FormatCoins(tpCost).c_str());
			const long long save = tpCost - craftCost;
			if (save >= 0)
				ImGui::TextColored(HelperTheme::Ok, "Savings vs buy: %s", FormatCoins(save).c_str());
			else
				ImGui::TextColored(HelperTheme::Warn, "Buy cheaper by: %s", FormatCoins(-save).c_str());
		}
		if (anyList)
		{
			ImGui::TextColored(HelperTheme::Muted, "Sell listed (net ~85%%): %s",
				FormatCoins(listNet).c_str());
			const long long profit = listNet - craftCost;
			if (profit >= 0)
				ImGui::TextColored(HelperTheme::Ok, "Profit (craft→list): %s",
					FormatCoins(profit).c_str());
			else
				ImGui::TextColored(HelperTheme::Warn, "Loss (craft→list): %s",
					FormatCoins(-profit).c_str());
			if (craftCost > 0)
				ImGui::TextColored(HelperTheme::Muted, "ROI: %lld%%",
					(profit * 100) / craftCost);
		}
		if (noTp > 0)
			ImGui::TextColored(HelperTheme::Warn, "Some mats have no TP price.");

		ImGui::Spacing();
		PadNav::SectionTitle("Shopping list");
		if (gOpts.groupByItem)
		{
			for (const Plan& p : plans)
			{
				if (!p.ok || p.shopping.empty()) continue;
				ImGui::TextColored(HelperTheme::GoldBright, "%s (x%d)",
					p.outputName.empty() ? "Item" : p.outputName.c_str(),
					(std::max)(1, p.wantQty));
				DrawShopRows(p.shopping, allowCartGot);
				ImGui::Spacing();
			}
		}
		else
		{
			std::vector<ShopRow> rows;
			for (auto& kv : shopAgg) rows.push_back(kv.second);
			std::sort(rows.begin(), rows.end(),
				[](const ShopRow& a, const ShopRow& b) { return a.total > b.total; });
			DrawShopRows(rows, allowCartGot);
		}

		ImGui::Spacing();
		PadNav::SectionTitle("Crafting steps");
		if (gOpts.groupByItem)
		{
			for (const Plan& p : plans)
			{
				if (!p.ok || p.steps.empty()) continue;
				ImGui::TextColored(HelperTheme::GoldBright, "%s",
					p.outputName.empty() ? "Item" : p.outputName.c_str());
				DrawStepsList(p.steps);
				ImGui::Spacing();
			}
		}
		else
			DrawStepsList(flatSteps);
	}

} // namespace CraftingDetail
