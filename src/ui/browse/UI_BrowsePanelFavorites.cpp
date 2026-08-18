#include "UI_Browse.h"
#include "UI_BrowseInternal.h"

#include "LivePanels.h"
#include "Sites.h"
#include "WikiBrowser.h"

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

	void DrawBookmarkRow(int folderId, int slotInFolder, int slot)
	{
		const char* title = Sites::FavoriteTitleAt(slot);
		const char* url = Sites::FavoriteUrlAt(slot);
		ImGui::PushID(slot);
		if (ImGui::Selectable(title && title[0] ? title : url, false))
		{
			if (url && url[0])
				WikiBrowser::Navigate(url);
		}
		if (ImGui::IsItemHovered() && url)
			ImGui::SetTooltip("%s", url);
		struct FavDrag
		{
			int folderId;
			int slot;
		};
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			FavDrag drag{ folderId, slotInFolder };
			ImGui::SetDragDropPayload("FAV_FOLDER_REORDER", &drag, sizeof(drag));
			ImGui::TextUnformatted(title ? title : "Bookmark");
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FAV_FOLDER_REORDER"))
			{
				const FavDrag from = *static_cast<const FavDrag*>(payload->Data);
				if (from.folderId == folderId && from.slot >= 0 &&
					Sites::MoveFavoriteInFolder(folderId, from.slot, slotInFolder))
					LivePanels::NotifyFavoritesChanged();
			}
			ImGui::EndDragDropTarget();
		}
		if (ImGui::BeginPopupContextItem("##gw2igh_fav_ctx"))
		{
			ImGui::TextDisabled("Move to folder");
			ImGui::Separator();
			if (ImGui::MenuItem("Unfiled", nullptr, folderId == 0))
			{
				if (Sites::SetFavoriteFolder(url, 0))
					LivePanels::NotifyFavoritesChanged();
			}
			const int folderN = Sites::FavoriteFolderCount();
			for (int fi = 0; fi < folderN; ++fi)
			{
				const int fid = Sites::FavoriteFolderIdAt(fi);
				const char* fname = Sites::FavoriteFolderName(fid);
				if (ImGui::MenuItem(fname ? fname : "Folder", nullptr, folderId == fid))
				{
					if (Sites::SetFavoriteFolder(url, fid))
						LivePanels::NotifyFavoritesChanged();
				}
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Delete"))
			{
				if (Sites::RemoveFavoriteSlot(slot))
					LivePanels::NotifyFavoritesChanged();
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();
	}

	void DrawFolderBlock(const BrowseSitesDrawCtx& ctx, int folderId)
	{
		(void)ctx;
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
				ImGui::SetTooltip("Delete folder - bookmarks inside return to Unfiled");
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
				const int src = Sites::FavoriteSlotInFolder(from.folderId, from.slot);
				if (src >= 0)
				{
					const char* url = Sites::FavoriteUrlAt(src);
					if (url && Sites::SetFavoriteFolder(url, folderId))
						LivePanels::NotifyFavoritesChanged();
				}
			}
			ImGui::EndDragDropTarget();
		}
		if (!open || n <= 0)
			return;
		for (int f = 0; f < n; ++f)
		{
			const int slot = Sites::FavoriteSlotInFolder(folderId, f);
			if (slot >= 0)
				DrawBookmarkRow(folderId, f, slot);
		}
	}
}

void DrawBrowseFavoritesHeader()
{
	ImGui::TextUnformatted("Bookmarks");
	ImGui::SameLine(0.f, 10.f);
	ImGui::PopStyleColor();
	if (ImGui::SmallButton("+ Folder###gw2igh_fav_add_folder"))
	{
		sNewFolder[0] = 0;
		sFocusNewFolder = true;
		ImGui::OpenPopup("##gw2igh_new_fav_folder");
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Create a folder");
	ImGui::PushStyleColor(ImGuiCol_Text, kGold);
}

void DrawBrowseFavoritesPane(const BrowseSitesDrawCtx& ctx)
{
	if (ImGui::BeginPopupModal("##gw2igh_new_fav_folder", nullptr,
		ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("New folder");
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

	DrawFolderBlock(ctx, 0);
	const int folderN = Sites::FavoriteFolderCount();
	for (int fi = 0; fi < folderN; ++fi)
		DrawFolderBlock(ctx, Sites::FavoriteFolderIdAt(fi));

	if (Sites::FavoriteCount() == 0)
		ImGui::TextDisabled("No bookmarks yet — star the address bar to pin this page.");
}

} // namespace UIBrowseDetail
