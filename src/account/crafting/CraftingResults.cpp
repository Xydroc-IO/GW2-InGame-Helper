#include "CraftingData.h"

#include "CraftingShared.h"

#include "EconomyInternal.h"
#include "EconomyShared.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadLayout.h"
#include "PadNav.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
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
		char label[288];
		const char* mode = n.crafted && !n.kids.empty() ? "craft" : (miss > 0 ? "buy" : "owned");
		std::snprintf(label, sizeof(label), "[%s] %s  %d / %d", mode, name, n.have, n.need);
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
		ImGui::TextColored(HelperTheme::Muted, "Plan options");
		bool dirty = false;
		bool firstOpt = true;
		auto check = [&](const char* label, bool* v) {
			const float w = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x
				+ PadNav::VisibleLabelWidth(label);
			if (!firstOpt)
				PadNav::WrapSameLine(w);
			firstOpt = false;
			if (ImGui::Checkbox(label, v))
				dirty = true;
		};
		check("Use owned materials###gw2igh_opt_own", &gOpts.useOwnMaterials);
		check("Craft sub-components###gw2igh_opt_sub", &gOpts.craftSubComponents);
		check("Group by item###gw2igh_opt_grp", &gOpts.groupByItem);
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
				ImGui::TextColored(HelperTheme::Ok, "Profit (craft to list): %s",
					FormatCoins(profit).c_str());
			else
				ImGui::TextColored(HelperTheme::Warn, "Loss (craft to list): %s",
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
		const int cols = allowCartGot ? 5 : 4;
		const float rowH = ImGui::GetFrameHeightWithSpacing();
		const float headerH = ImGui::GetTextLineHeightWithSpacing() + 6.f;
		const int n = static_cast<int>(rows.size());
		const float need = headerH + rowH * static_cast<float>(n) + 6.f;
		/* Leave room for checked/need lines, cart buttons, and the headers below. */
		const float reserve = rowH * 5.5f + 36.f;
		float maxH = ImGui::GetContentRegionAvail().y - reserve;
		if (maxH < rowH * 6.f)
			maxH = rowH * 6.f;
		const bool scroll = need > maxH + 1.f;
		const float tableH = scroll ? maxH : need;
		ImGuiTableFlags tflags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
			| ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_PadOuterX;
		if (scroll)
			tflags |= ImGuiTableFlags_ScrollY;
		if (ImGui::BeginTable("###gw2igh_craft_shop", cols, tflags, ImVec2(0.f, tableH)))
		{
			if (allowCartGot)
				ImGui::TableSetupColumn("##got", ImGuiTableColumnFlags_WidthFixed, 22.f);
			ImGui::TableSetupColumn("Material", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthFixed, 48.f);
			ImGui::TableSetupColumn("Unit", ImGuiTableColumnFlags_WidthFixed, 74.f);
			ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, 78.f);
			ImGui::TableHeadersRow();
			for (const ShopRow& s : rows)
			{
				const bool got = allowCartGot && CartIsGot(s.itemId, nullptr);
				if (got) ++checked;
				else if (s.priced && s.total >= 0)
					stillNeed += s.total;

				ImGui::PushID(s.itemId);
				ImGui::TableNextRow();
				int col = 0;
				if (allowCartGot)
				{
					ImGui::TableSetColumnIndex(col++);
					bool mark = got;
					if (ImGui::Checkbox("##got", &mark))
						CartSetGot(s.itemId, mark, nullptr);
				}
				ImGui::TableSetColumnIndex(col++);
				if (got)
					ImGui::TextColored(HelperTheme::Muted, "%s",
						s.name.empty() ? "Item" : s.name.c_str());
				else
					ImGui::TextColored(HelperTheme::GoldBright, "%s",
						s.name.empty() ? "Item" : s.name.c_str());
				ImGui::TableSetColumnIndex(col++);
				ImGui::TextColored(got ? HelperTheme::Muted : HelperTheme::Ink, "x%d", s.qty);
				ImGui::TableSetColumnIndex(col++);
				if (got)
					ImGui::TextColored(HelperTheme::Muted, "—");
				else if (s.priced)
					ImGui::TextColored(HelperTheme::Muted, "%s",
						FormatCoins(s.unitSell).c_str());
				else
					ImGui::TextColored(HelperTheme::Warn, "no TP");
				ImGui::TableSetColumnIndex(col++);
				if (got)
					ImGui::TextColored(HelperTheme::Ok, "got");
				else if (s.priced)
					ImGui::TextColored(HelperTheme::Ink, "%s",
						FormatCoins(s.total).c_str());
				else
					ImGui::TextColored(HelperTheme::Warn, "—");
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		if (allowCartGot)
			ImGui::TextColored(HelperTheme::Muted, "%d / %d checked",
				checked, static_cast<int>(rows.size()));
		if (stillNeed > 0)
			ImGui::TextColored(HelperTheme::Ink, "Still need: %s",
				FormatCoins(stillNeed).c_str());
		if (PadLayout::GoldButton("Add missing to Economy cart###gw2igh_craft_ecart", true, true))
		{
			for (const ShopRow& s : rows)
			{
				if (allowCartGot && CartIsGot(s.itemId, nullptr)) continue;
				if (s.qty <= 0) continue;
				EconomyDetail::AddToCart(s.itemId,
					s.name.empty() ? "Item" : s.name.c_str(), s.qty);
			}
			G::ShowEconomy = true;
			EconomyDetail::gTab = EconomyDetail::kTabCart;
			EconomyDetail::gForceTab = EconomyDetail::kTabCart;
			Settings::SetDirty();
		}
		if (allowCartGot)
		{
			if (PadLayout::GoldButton("Clear checks###gw2igh_craft_clrgot", false, false))
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
		const char* outName = plan.outputName.empty() ? "Output" : plan.outputName.c_str();
		PadLayout::Hero("###gw2igh_craft_plan_hero", "Craft cost", outName,
			FormatCoins(plan.buyTotal).c_str());

		const bool forgeWarn = plan.recipeDiscipline.find("Mystic") != std::string::npos
			|| plan.recipeSource.find("wiki") != std::string::npos
			|| plan.recipeSource.find("forge") != std::string::npos;
		if (forgeWarn)
			PadNav::StatusWarn("Forge / wiki acquisition — verify steps in-game.");
		if (plan.noTpMissing > 0)
			PadNav::StatusWarn("Some mats have no TP price — craft cost is a lower bound.");

		ImGui::Spacing();
		PadNav::SectionTitle("Shopping list");
		DrawShopRows(plan.shopping, allowCartGot);

		if (ImGui::CollapsingHeader("Details###gw2igh_craft_details"))
		{
			const char* src = plan.recipeSource.empty() ? "recipe" : plan.recipeSource.c_str();
			const char* disc = plan.recipeDiscipline.empty() ? nullptr : plan.recipeDiscipline.c_str();
			if (disc && std::strcmp(disc, src) == 0)
				disc = nullptr;
			if (disc)
				ImGui::TextColored(HelperTheme::Muted,
					"#%d · %s · %s · want x%d · yield %d",
					plan.outputId, src, disc,
					(std::max)(1, plan.wantQty), plan.outputCount);
			else
				ImGui::TextColored(HelperTheme::Muted,
					"#%d · %s · want x%d · yield %d",
					plan.outputId, src,
					(std::max)(1, plan.wantQty), plan.outputCount);
			DrawKnownBadge(plan);
		}

		if (ImGui::CollapsingHeader("Financial breakdown###gw2igh_craft_fin"))
			DrawFinancialOne(plan);

		if (ImGui::CollapsingHeader("Crafting steps###gw2igh_craft_steps"))
			DrawStepsList(plan.steps);

		if (ImGui::CollapsingHeader("Recipe tree###gw2igh_craft_tree"))
		{
			for (const IngNode& k : plan.root.kids)
				DrawNode(k);
		}
	}

	void DrawAggregatedResults(const std::vector<Plan>& plans, bool allowCartGot)
	{
		long long craftCost = 0, tpCost = 0, listNet = 0;
		bool anyTp = false, anyList = false;
		int noTp = 0;
		int okCount = 0;
		std::unordered_map<int, ShopRow> shopAgg;
		std::vector<StepRow> flatSteps;

		for (const Plan& p : plans)
		{
			if (!p.ok)
				continue;
			++okCount;
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

		char projTitle[64];
		std::snprintf(projTitle, sizeof(projTitle), "%d item%s in project",
			okCount, okCount == 1 ? "" : "s");
		PadLayout::Hero("###gw2igh_craft_proj_hero", "Project craft cost", projTitle,
			FormatCoins(craftCost).c_str());

		for (const Plan& p : plans)
		{
			if (p.ok) continue;
			ImGui::TextColored(HelperTheme::Warn, "%s — %s",
				p.outputName.empty() ? "Item" : p.outputName.c_str(),
				p.status.c_str());
		}
		if (noTp > 0)
			PadNav::StatusWarn("Some mats have no TP price.");

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

		if (ImGui::CollapsingHeader("Financial breakdown###gw2igh_craft_proj_fin"))
		{
			ImGui::TextColored(HelperTheme::Ink, "Craft cost:  %s",
				FormatCoins(craftCost).c_str());
			if (anyTp)
			{
				ImGui::TextColored(HelperTheme::Ink, "Buy outright: %s",
					FormatCoins(tpCost).c_str());
				const long long save = tpCost - craftCost;
				if (save >= 0)
					ImGui::TextColored(HelperTheme::Ok, "Savings vs buy: %s",
						FormatCoins(save).c_str());
				else
					ImGui::TextColored(HelperTheme::Warn, "Buy cheaper by: %s",
						FormatCoins(-save).c_str());
			}
			if (anyList)
			{
				ImGui::TextColored(HelperTheme::Muted, "Sell listed (net ~85%%): %s",
					FormatCoins(listNet).c_str());
				const long long profit = listNet - craftCost;
				if (profit >= 0)
					ImGui::TextColored(HelperTheme::Ok, "Profit (craft to list): %s",
						FormatCoins(profit).c_str());
				else
					ImGui::TextColored(HelperTheme::Warn, "Loss (craft to list): %s",
						FormatCoins(-profit).c_str());
				if (craftCost > 0)
					ImGui::TextColored(HelperTheme::Muted, "ROI: %lld%%",
						(profit * 100) / craftCost);
			}
		}

		if (ImGui::CollapsingHeader("Crafting steps###gw2igh_craft_proj_steps"))
		{
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
	}

} // namespace CraftingDetail
