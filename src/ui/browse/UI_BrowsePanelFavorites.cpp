#include "UI_Browse.h"
#include "UI_BrowseInternal.h"

#include "LivePanels.h"
#include "Sites.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace UIBrowseDetail
{
namespace
{
	char sNewFolder[48]{};
	bool sFocusNewFolder = false;
	int sRenameFolderId = -1;
	char sRenameBuf[48]{};

	void DrawFolderBlock(const BrowseSitesDrawCtx& ctx, int folderId)
	{
		const int n = Sites::FavoriteCountInFolder(folderId);
		const char* name = Sites::FavoriteFolderName(folderId);
		if (folderId != 0)
		{
			char delId[48];
			std::snprintf(delId, sizeof(delId), "Del###gw2igh_favdel_%d", folderId);
			if (ImGui::SmallButton(delId))
			{
				if (Sites::DeleteFavoriteFolder(folderId))
					LivePanels::NotifyFavoritesChanged();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Delete folder - favorites inside return to Unfiled");
			ImGui::SameLine(0.f, 6.f);
		}
		char header[96];
		std::snprintf(header, sizeof(header), "%s (%d)###gw2igh_favfold_%d",
			name ? name : "Folder", n, folderId);
		const bool open = ImGui::CollapsingHeader(header,
			ImGuiTreeNodeFlags_DefaultOpen);
		if (folderId != 0 && ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Rename..."))
			{
				sRenameFolderId = folderId;
				std::snprintf(sRenameBuf, sizeof(sRenameBuf), "%s", name ? name : "");
			}
			if (ImGui::MenuItem("Delete folder (items -> Unfiled)"))
			{
				if (Sites::DeleteFavoriteFolder(folderId))
					LivePanels::NotifyFavoritesChanged();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FAV_FOLDER_REORDER"))
			{
				struct FavDrag { int folderId; int slot; };
				const FavDrag from = *static_cast<const FavDrag*>(payload->Data);
				/* Drop on header: move that item into this folder. */
				const int si = Sites::FavoriteSiteIndexInFolder(from.folderId, from.slot);
				if (si >= 0 && ctx.sites)
				{
					const char* id = ctx.sites[si].id;
					if (id && Sites::SetFavoriteFolder(id, folderId))
						LivePanels::NotifyFavoritesChanged();
				}
			}
			ImGui::EndDragDropTarget();
		}
		if (!open || n <= 0)
			return;
		std::vector<int> idxs;
		idxs.reserve(static_cast<size_t>(n));
		for (int f = 0; f < n; ++f)
		{
			const int si = Sites::FavoriteSiteIndexInFolder(folderId, f);
			if (si >= 0)
				idxs.push_back(si);
		}
		DrawBrowseClippedRows(ctx, idxs, true);
	}
}

void DrawBrowseFavoritesHeader()
{
	/* Caller has already PushStyleColor(ImGuiCol_Text, kGold). */
	ImGui::TextUnformatted("Favorites");
	ImGui::SameLine(0.f, 10.f);
	ImGui::PopStyleColor();
	if (ImGui::SmallButton("+ Folder###gw2igh_fav_add_folder"))
	{
		sNewFolder[0] = 0;
		sFocusNewFolder = true;
		ImGui::OpenPopup("##gw2igh_new_fav_folder");
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Create a folder to organize favorites");
	ImGui::PushStyleColor(ImGuiCol_Text, kGold);
}

void DrawBrowseFavoritesPane(const BrowseSitesDrawCtx& ctx)
{
	if (ImGui::BeginPopupModal("##gw2igh_new_fav_folder", nullptr,
		ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("New favorites folder");
		ImGui::Spacing();
		if (sFocusNewFolder)
		{
			ImGui::SetKeyboardFocusHere();
			sFocusNewFolder = false;
		}
		const bool enter = ImGui::InputTextWithHint("##gw2igh_new_fav_name",
			"Folder name...", sNewFolder, sizeof(sNewFolder),
			ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::Spacing();
		const bool canSave = sNewFolder[0] != 0;
		if ((ImGui::Button("Create", ImVec2(96.f, 0.f)) || enter) && canSave)
		{
			if (Sites::CreateFavoriteFolder(sNewFolder))
			{
				sNewFolder[0] = 0;
				LivePanels::NotifyFavoritesChanged();
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(96.f, 0.f)))
		{
			sNewFolder[0] = 0;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (sRenameFolderId > 0)
	{
		ImGui::OpenPopup("##gw2igh_rename_fav_folder");
		if (ImGui::BeginPopupModal("##gw2igh_rename_fav_folder", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Rename folder");
			ImGui::InputText("##gw2igh_ren_fav", sRenameBuf, sizeof(sRenameBuf));
			if (ImGui::Button("Save") && sRenameBuf[0])
			{
				if (Sites::RenameFavoriteFolder(sRenameFolderId, sRenameBuf))
					LivePanels::NotifyFavoritesChanged();
				sRenameFolderId = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				sRenameFolderId = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	/* Unfiled first, then user folders. */
	DrawFolderBlock(ctx, 0);
	const int folderN = Sites::FavoriteFolderCount();
	for (int fi = 0; fi < folderN; ++fi)
		DrawFolderBlock(ctx, Sites::FavoriteFolderIdAt(fi));

	if (Sites::FavoriteCount() == 0)
		ImGui::TextDisabled("No favorites yet - star a site to pin it here.");
}

} // namespace UIBrowseDetail
