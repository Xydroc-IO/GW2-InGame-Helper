#include "UI.h"
#include "UIInternal.h"

#include "LivePanels.h"
#include "Sites.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <cstdio>
#include <cstring>

namespace UIDetail
{
namespace
{
	char sNewFolder[48]{};
	bool sFocusNewFolder = false;
	int sRenameFolderId = -1;
	char sRenameBuf[48]{};
	int sRenameBookmarkSlot = -1;
	char sRenameBookmark[64]{};

	struct BarDrag
	{
		int kind = 0; /* 0 = bookmark global slot, 1 = folder index */
		int a = -1;
		int b = -1;
	};

	bool ReadBarDrag(const ImGuiPayload* payload, BarDrag* out)
	{
		if (!payload || !payload->Data || !out || payload->DataSize != sizeof(BarDrag))
			return false;
		*out = *static_cast<const BarDrag*>(payload->Data);
		return true;
	}

	void OpenUrlSlot(int slot)
	{
		const char* url = Sites::FavoriteUrlAt(slot);
		if (url && url[0])
			WikiBrowser::Navigate(url);
	}

	void FolderContext(int folderId, int folderIndex)
	{
		const char* name = Sites::FavoriteFolderName(folderId);
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Rename..."))
			{
				sRenameFolderId = folderId;
				std::snprintf(sRenameBuf, sizeof(sRenameBuf), "%s", name ? name : "");
			}
			if (ImGui::MenuItem("Delete folder (items -> bar)"))
			{
				if (Sites::DeleteFavoriteFolder(folderId))
					LivePanels::NotifyFavoritesChanged();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			BarDrag d{ 1, folderIndex, folderId };
			ImGui::SetDragDropPayload("GW2IGH_BOOKMARK_BAR", &d, sizeof(d));
			ImGui::TextUnformatted(name ? name : "Folder");
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GW2IGH_BOOKMARK_BAR"))
			{
				BarDrag from{};
				if (ReadBarDrag(payload, &from))
				{
					if (from.kind == 1 && from.a >= 0)
					{
						if (Sites::MoveFavoriteFolder(from.a, folderIndex))
							LivePanels::NotifyFavoritesChanged();
					}
					else if (from.kind == 0 && from.a >= 0)
					{
						if (Sites::SetFavoriteFolder(Sites::FavoriteUrlAt(from.a), folderId))
							LivePanels::NotifyFavoritesChanged();
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
	}

	void BookmarkContext(int slot, int folderId, int slotInFolder)
	{
		const char* title = Sites::FavoriteTitleAt(slot);
		const char* url = Sites::FavoriteUrlAt(slot);
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Open"))
				OpenUrlSlot(slot);
			if (ImGui::MenuItem("Rename..."))
			{
				sRenameBookmarkSlot = slot;
				std::snprintf(sRenameBookmark, sizeof(sRenameBookmark), "%s",
					title ? title : "");
			}
			ImGui::Separator();
			ImGui::TextDisabled("Move to folder");
			if (ImGui::MenuItem("Bookmarks bar", nullptr, folderId == 0))
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
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			BarDrag d{ 0, slot, slotInFolder };
			ImGui::SetDragDropPayload("GW2IGH_BOOKMARK_BAR", &d, sizeof(d));
			ImGui::TextUnformatted(title ? title : "Bookmark");
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GW2IGH_BOOKMARK_BAR"))
			{
				BarDrag from{};
				if (ReadBarDrag(payload, &from) && from.kind == 0 && from.b >= 0 &&
					slotInFolder >= 0 &&
					Sites::MoveFavoriteInFolder(folderId, from.b, slotInFolder))
					LivePanels::NotifyFavoritesChanged();
			}
			ImGui::EndDragDropTarget();
		}
	}

	bool DrawChip(const char* label, const char* id)
	{
		char buf[96];
		std::snprintf(buf, sizeof(buf), "%s###%s", label && label[0] ? label : "…", id);
		return ImGui::SmallButton(buf);
	}

	void DrawModals()
	{
		if (sFocusNewFolder)
			ImGui::OpenPopup("##gw2igh_bar_new_folder");
		if (ImGui::BeginPopupModal("##gw2igh_bar_new_folder", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("New folder");
			if (sFocusNewFolder)
			{
				ImGui::SetKeyboardFocusHere();
				sFocusNewFolder = false;
			}
			const bool enter = ImGui::InputText("##gw2igh_bar_fn", sNewFolder, sizeof(sNewFolder),
				ImGuiInputTextFlags_EnterReturnsTrue);
			if ((ImGui::Button("Create") || enter) && sNewFolder[0])
			{
				if (Sites::CreateFavoriteFolder(sNewFolder))
					LivePanels::NotifyFavoritesChanged();
				sNewFolder[0] = 0;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				sNewFolder[0] = 0;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (sRenameFolderId > 0)
			ImGui::OpenPopup("##gw2igh_bar_ren_folder");
		if (ImGui::BeginPopupModal("##gw2igh_bar_ren_folder", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Rename folder");
			ImGui::InputText("##gw2igh_bar_ren", sRenameBuf, sizeof(sRenameBuf));
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
		if (sRenameBookmarkSlot >= 0)
			ImGui::OpenPopup("##gw2igh_bar_ren_bm");
		if (ImGui::BeginPopupModal("##gw2igh_bar_ren_bm", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Rename bookmark");
			ImGui::InputText("##gw2igh_bar_bm", sRenameBookmark, sizeof(sRenameBookmark));
			if (ImGui::Button("Save") && sRenameBookmark[0])
			{
				if (Sites::RenameFavorite(sRenameBookmarkSlot, sRenameBookmark))
					LivePanels::NotifyFavoritesChanged();
				sRenameBookmarkSlot = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				sRenameBookmarkSlot = -1;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}
}

	void DrawBookmarkBar()
	{
		DrawModals();
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 2.f));
		const float maxX = ImGui::GetWindowContentRegionMax().x;
		bool overflow = false;
		int shown = 0;

		auto room = [&]() -> bool {
			if (overflow)
				return false;
			if (ImGui::GetCursorPosX() > maxX - 72.f)
			{
				overflow = true;
				return false;
			}
			return true;
		};

		const int unfiledN = Sites::FavoriteCountInFolder(0);
		for (int i = 0; i < unfiledN; ++i)
		{
			const int slot = Sites::FavoriteSlotInFolder(0, i);
			if (slot < 0)
				continue;
			if (!room())
				break;
			if (shown)
				ImGui::SameLine();
			char id[40];
			std::snprintf(id, sizeof(id), "bm%d", slot);
			if (DrawChip(Sites::FavoriteTitleAt(slot), id))
				OpenUrlSlot(slot);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", Sites::FavoriteUrlAt(slot));
			BookmarkContext(slot, 0, i);
			++shown;
		}

		const int folderN = Sites::FavoriteFolderCount();
		for (int fi = 0; fi < folderN; ++fi)
		{
			if (!room())
				break;
			const int fid = Sites::FavoriteFolderIdAt(fi);
			if (shown)
				ImGui::SameLine();
			char id[40];
			std::snprintf(id, sizeof(id), "fold%d", fid);
			char lab[64];
			std::snprintf(lab, sizeof(lab), "%s ▾", Sites::FavoriteFolderName(fid));
			if (DrawChip(lab, id))
				ImGui::OpenPopup(id);
			FolderContext(fid, fi);
			if (ImGui::BeginPopup(id))
			{
				const int n = Sites::FavoriteCountInFolder(fid);
				if (n <= 0)
					ImGui::TextDisabled("Empty folder");
				for (int i = 0; i < n; ++i)
				{
					const int slot = Sites::FavoriteSlotInFolder(fid, i);
					if (slot < 0)
						continue;
					if (ImGui::MenuItem(Sites::FavoriteTitleAt(slot)))
						OpenUrlSlot(slot);
					BookmarkContext(slot, fid, i);
				}
				ImGui::EndPopup();
			}
			++shown;
		}

		if (overflow)
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("»###gw2igh_bm_more"))
				ImGui::OpenPopup("##gw2igh_bm_overflow");
			if (ImGui::BeginPopup("##gw2igh_bm_overflow"))
			{
				ImGui::TextDisabled("More bookmarks");
				ImGui::Separator();
				for (int i = 0; i < Sites::FavoriteCount(); ++i)
				{
					if (ImGui::MenuItem(Sites::FavoriteTitleAt(i)))
						OpenUrlSlot(i);
				}
				ImGui::EndPopup();
			}
			++shown;
		}

		if (shown)
			ImGui::SameLine();
		if (ImGui::SmallButton("+###gw2igh_bm_addfold"))
		{
			sNewFolder[0] = 0;
			sFocusNewFolder = true;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("New folder");
		if (ImGui::BeginPopupContextItem("##gw2igh_bm_bar_bg"))
		{
			if (ImGui::MenuItem("New folder"))
			{
				sNewFolder[0] = 0;
				sFocusNewFolder = true;
			}
			ImGui::EndPopup();
		}
		if (Sites::FavoriteCount() == 0 && Sites::FavoriteFolderCount() == 0)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("Star a page to bookmark it");
		}
		ImGui::PopStyleVar();
	}
} // namespace UIDetail
