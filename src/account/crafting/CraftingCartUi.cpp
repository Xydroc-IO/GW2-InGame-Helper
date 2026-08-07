#include "CraftingData.h"

#include "CraftingShared.h"

#include "HelperTheme.h"
#include "PadNav.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace CraftingDetail
{
	static char gNewProjName[64] = {};
	static char gRenameBuf[64] = {};
	static bool gRenaming = false;

	void DrawCartUi()
	{
		CartEnsureLoaded();
		PadNav::SectionTitle("Craft cart");
		ImGui::TextColored(HelperTheme::Muted,
			"Named multi-item projects with check-off shopping.");

		std::vector<std::string> names = CartProjectNames();
		int activeIdx = 0;
		for (size_t i = 0; i < names.size(); ++i)
			if (names[i] == CartActiveName())
				activeIdx = static_cast<int>(i);

		std::vector<const char*> labels;
		for (const std::string& n : names)
			labels.push_back(n.c_str());
		if (!labels.empty())
		{
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
			if (ImGui::Combo("###gw2igh_cart_proj", &activeIdx, labels.data(),
					static_cast<int>(labels.size())))
				CartSetActive(names[static_cast<size_t>(activeIdx)].c_str());
		}

		ImGui::SameLine();
		if (ImGui::SmallButton("New###gw2igh_cart_new"))
		{
			const char* seed = gNewProjName[0] ? gNewProjName : "Project";
			CartNew(seed);
			gNewProjName[0] = 0;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Rename###gw2igh_cart_rn"))
		{
			gRenaming = true;
			std::snprintf(gRenameBuf, sizeof(gRenameBuf), "%s", CartActiveName());
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Delete###gw2igh_cart_del") && names.size() > 0)
			CartDelete(CartActiveName());

		if (gRenaming)
		{
			ImGui::InputText("###gw2igh_cart_rnb", gRenameBuf, sizeof(gRenameBuf));
			ImGui::SameLine();
			if (ImGui::SmallButton("OK###gw2igh_cart_rnok"))
			{
				CartRename(CartActiveName(), gRenameBuf);
				gRenaming = false;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Cancel###gw2igh_cart_rnc"))
				gRenaming = false;
		}

		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
		ImGui::InputTextWithHint("###gw2igh_cart_nn", "New project name",
			gNewProjName, sizeof(gNewProjName));

		{
			std::lock_guard<std::mutex> lock(gMu);
			if (gPlan.ok && gPlan.outputId > 0)
			{
				if (ImGui::SmallButton("Add current plan###gw2igh_cart_addplan"))
					CartAdd(gPlan.outputId,
						gPlan.outputName.empty() ? "Item" : gPlan.outputName.c_str(), 1, nullptr);
			}
		}

		ImGui::SameLine();
		if (ImGui::SmallButton("Clear project###gw2igh_cart_clr"))
			CartClear(nullptr);

		std::vector<CartItem> items = CartItems(nullptr);
		if (items.empty())
		{
			ImGui::TextColored(HelperTheme::Muted,
				"Cart empty — add from Known (+), current plan, or Economy Plan.");
			return;
		}

		for (size_t i = 0; i < items.size(); ++i)
		{
			CartItem& it = items[i];
			ImGui::PushID(static_cast<int>(it.id * 1000 + static_cast<int>(i)));
			ImGui::TextWrapped("%s  x%d  (#%d)", it.name, it.qty, it.id);
			if (ImGui::SmallButton("-") && it.qty > 1)
				CartSetQty(it.id, it.qty - 1, nullptr);
			ImGui::SameLine();
			if (ImGui::SmallButton("+"))
				CartSetQty(it.id, it.qty + 1, nullptr);
			ImGui::SameLine();
			if (ImGui::SmallButton("Plan"))
			{
				if (it.name[0])
					std::snprintf(gQuery, sizeof(gQuery), "%s", it.name);
				else
					std::snprintf(gQuery, sizeof(gQuery), "%d", it.id);
				StartPlanWithQty(it.qty < 1 ? 1 : it.qty);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("X"))
				CartRemove(it.id, nullptr);
			ImGui::PopID();
		}

		if (ImGui::Button("Plan project###gw2igh_cart_planall"))
			StartCartProjectPlan();
		ImGui::SameLine();
		if (ImGui::Button("Plan first###gw2igh_cart_plan1") && !items.empty())
		{
			const CartItem& it = items.front();
			if (it.name[0])
				std::snprintf(gQuery, sizeof(gQuery), "%s", it.name);
			else
				std::snprintf(gQuery, sizeof(gQuery), "%d", it.id);
			StartPlanWithQty(it.qty < 1 ? 1 : it.qty);
		}
	}

} // namespace CraftingDetail
