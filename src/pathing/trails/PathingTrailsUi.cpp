#include "PathingTrails.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "PathingPacks.h"
#include "PathingIndex.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include <windows.h>

#include "imgui/imgui.h"

using namespace PathingDetail;

bool PathingTrails::DrawOverlaySettings()
{
	bool dirty = false;
	uint32_t mapId = 0;
	if (G::Mumble && G::Mumble->uiTick != 0)
	{
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		if (ctx)
			mapId = ctx->mapId;
	}
	Update(mapId ? mapId : 1u);

	dirty |= ImGui::Checkbox("Enable path overlays", &G::ShowPathingTrails);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Master switch - allows compass / world drawing (packs still index below).");
	if (G::ShowPathingTrails)
	{
		dirty |= ImGui::Checkbox("Draw on in-game compass", &G::ShowCompassOverlay);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("TacO / Blish style - project enabled markers onto the stock compass.");
		dirty |= ImGui::Checkbox("In-world GPS trails", &G::ShowWorldTrails);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("3D world breadcrumbs near you (same categories as the compass).");

		/* Always visible on Overview under the GPS checkbox. */
		ImGui::Indent();
		ImGui::TextUnformatted("In-world GPS");
		if (!G::ShowWorldTrails)
		{
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted,
				"Enable \"In-world GPS trails\" above to apply.");
			PadNav::PopWrap();
		}
		PadNav::PushLabeledItemWidth();
		dirty |= ImGui::SliderFloat("GPS range (m)", &G::WorldTrailMaxDist, 40.f, 200.f, "%.0f");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"How far in-world GPS ribbons stay visible around you.\n"
				"Lower = shorter trails; higher = longer corridors.");
		dirty |= ImGui::SliderFloat("GPS width", &G::WorldTrailWidth, 0.5f, 4.0f, "%.1fx");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Multiplier on pack trailScale (1.0 = Blish/TacO default).");
		dirty |= ImGui::SliderFloat("World markers", &G::WorldMarkerScale, 0.5f, 3.0f, "%.1fx");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Scale pack icons in the 3D world (in-world GPS).\n"
				"2.0x = default.");
		dirty |= ImGui::SliderFloat("Compass icons", &G::CompassMarkerScale, 0.5f, 3.0f, "%.1fx");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Scale pack icons on the stock in-game compass.\n"
				"1.0x = default.");
		dirty |= ImGui::SliderFloat("Player clear", &G::WorldTrailPlayerClear, 0.f, 3.0f, "%.1fx");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Fade trails away from you.\n"
				"0 = full path visible (can draw over you).\n"
				"1 = default gap | higher = larger clear bubble.");
		dirty |= ImGui::SliderFloat("Marker clear", &G::WorldMarkerPlayerClear, 0.f, 3.0f, "%.1fx");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip(
				"Fade world markers near you (occlusion / soft-clear).\n"
				"0 = keep icons at your feet.\n"
				"1 ~ 2-5.5 m | higher = larger hole.\n"
				"Mount / Barefoot shortcut icons use a smaller bubble.");
		PadNav::PopLabeledItemWidth();
		ImGui::Unindent();

		dirty |= ImGui::Checkbox("Hide when world map open", &G::HideWhenMapOpen);
		dirty |= ImGui::Checkbox("Hide out of gameplay", &G::HideOutOfGameplay);
	}
	return dirty;
}

bool PathingTrails::DrawPackTools()
{
	bool dirty = false;
	uint32_t mapId = 0;
	if (G::Mumble && G::Mumble->uiTick != 0)
	{
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		if (ctx)
			mapId = ctx->mapId;
	}
	Update(mapId ? mapId : 1u);

	ImGui::TextUnformatted("Packs");
	const std::string pathHint = PathingFolderHint();
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted, "%s", pathHint.c_str());
	PadNav::PopWrap();
	if (ImGui::Button("Reload packs"))
		ReloadPacks();
	PadNav::WrapSameLine(PadNav::ButtonWidth("Update curated"));
	if (ImGui::Button("Update curated"))
		UpdateCuratedPacks();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Re-download latest Lady Elyssa + Hero + Tekkit packs from GitHub / Tekkit CDN.\n"
			"Does not remove any .taco you added yourself.");
	PadNav::WrapSameLine(PadNav::ButtonWidth("Open folder"));
	if (ImGui::Button("Open folder"))
		OpenPathingFolder();
	if (IsLoading() || PathingPacks::IsUpdating())
	{
		PadNav::WrapSameLine(ImGui::CalcTextSize("Updating...").x);
		ImGui::TextColored(HelperTheme::Muted,
			PathingPacks::IsUpdating() ? "Updating..." : "Loading...");
	}
	{
		char st[160]{};
		PathingPacks::GetStatus(st, sizeof(st));
		if (st[0])
		{
			PadNav::PushWrap();
			ImGui::TextColored(HelperTheme::Muted, "%s", st);
			PadNav::PopWrap();
		}
	}

	const bool loading = IsLoading() || PathingPacks::IsUpdating();
	const std::vector<std::string> packs = LoadedPackNames();
	PadNav::PushWrap();
	if (loading)
		ImGui::TextColored(HelperTheme::Muted,
			"Packs: %d | indexing categories...", PackCount());
	else
		ImGui::TextColored(HelperTheme::Muted,
			"Packs: %d | This map: %d trails, %d markers on",
			PackCount(), TrailCount(), MarkerCount());
	PadNav::PopWrap();
	if (!packs.empty())
	{
		/* Fill remaining Overview height - no tiny clipped list. */
		ImGui::BeginChild("##igh_tekkit_packs", ImVec2(0.f, 0.f), true);
		for (const std::string& name : packs)
			ImGui::BulletText("%s", name.c_str());
		ImGui::EndChild();
	}
	if (PackCount() == 0 && !loading)
	{
		PadNav::PushWrap();
		ImGui::TextColored(ImVec4(1.f, 0.7f, 0.3f, 1.f),
			"No .taco packs yet - click Update curated, or drop packs into the pathing folder.");
		PadNav::PopWrap();
	}
	(void)dirty;
	return false;
}

bool PathingTrails::DrawCategoryBrowser()
{
	bool dirty = false;
	uint32_t mapId = 0;
	if (G::Mumble && G::Mumble->uiTick != 0)
	{
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		if (ctx)
			mapId = ctx->mapId;
	}
	Update(mapId ? mapId : 1u);

	const bool loading = IsLoading() || PathingPacks::IsUpdating();
	ImGui::TextUnformatted("Categories");
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"Check to enable. Open drills into children.");
	PadNav::PopWrap();

	static char sFilter[96]{};
	ImGui::SetNextItemWidth(-1.f);
	ImGui::InputTextWithHint("###gw2igh_tekkit_filter", "Filter categories...", sFilter, sizeof(sFilter));
	if (ImGui::IsItemActive())
	{
		ImGui::GetIO().WantCaptureKeyboard = true;
		ImGui::CaptureKeyboardFromApp(true);
	}

	static uint64_t sTreeRevision = 0;
	static std::vector<Category> tree;
	static std::vector<std::string> sDrill; /* Category.path stack for breadcrumb */
	const uint64_t revision = gMenuRevision.load(std::memory_order_acquire);
	if (revision != sTreeRevision)
	{
		std::vector<Category> next = CategoryTree();
		if (!next.empty())
		{
			tree = std::move(next);
			sTreeRevision = revision;
		}
		else if (!IsLoading())
		{
			tree.clear();
			sTreeRevision = revision;
			sDrill.clear();
		}
	}
	if (tree.empty())
	{
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			loading ? "Indexing menu..." : "No categories yet - wait for pack index.");
		PadNav::PopWrap();
		return dirty;
	}

	std::function<Category*(std::vector<Category>&, const std::string&)> findNode =
		[&](std::vector<Category>& nodes, const std::string& path) -> Category* {
		for (Category& c : nodes)
		{
			if (c.path == path)
				return &c;
			if (Category* ch = findNode(c.children, path))
				return ch;
		}
		return nullptr;
	};

	std::vector<Category>* level = &tree;
	Category* current = nullptr;
	if (!sDrill.empty())
	{
		current = findNode(tree, sDrill.back());
		if (!current)
			sDrill.clear();
		else
			level = &current->children;
	}

	/* Breadcrumb - wraps to new rows (no single-line clip / scroll arrows). */
	if (PadNav::WrapButton("Root###gw2igh_cat_root", sDrill.empty(), /*first=*/true))
		sDrill.clear();
	for (size_t i = 0; i < sDrill.size(); ++i)
	{
		PadNav::WrapSlash();
		Category* n = findNode(tree, sDrill[i]);
		char lab[160];
		std::snprintf(lab, sizeof(lab), "%s###gw2igh_bc_%zu",
			n ? n->label.c_str() : sDrill[i].c_str(), i);
		if (PadNav::WrapButton(lab, i + 1 == sDrill.size()))
			sDrill.resize(i + 1);
	}
	if (!sDrill.empty())
	{
		if (PadNav::WrapButton("Back###gw2igh_cat_back", false))
			sDrill.pop_back();
	}

	const bool filterOn = sFilter[0] != 0;
	static char sFilterBuilt[96]{};
	static uint64_t sFilterTreeRev = 0;
	static std::unordered_set<std::string> sFilterShow;
	if (!filterOn)
	{
		sFilterBuilt[0] = 0;
		sFilterShow.clear();
		sFilterTreeRev = 0;
	}
	else if (std::strcmp(sFilter, sFilterBuilt) != 0 || sFilterTreeRev != sTreeRevision)
	{
		std::memcpy(sFilterBuilt, sFilter, sizeof(sFilterBuilt));
		sFilterTreeRev = sTreeRevision;
		sFilterShow.clear();
		auto toLower = [](std::string s) {
			for (char& ch : s)
				if (ch >= 'A' && ch <= 'Z')
					ch = static_cast<char>(ch - 'A' + 'a');
			return s;
		};
		const std::string needle = toLower(sFilter);
		std::function<bool(Category&)> mark = [&](Category& c) -> bool {
			bool hit = toLower(c.label).find(needle) != std::string::npos ||
				toLower(c.path).find(needle) != std::string::npos;
			bool childHit = false;
			for (Category& ch : c.children)
				childHit = mark(ch) || childHit;
			if (hit || childHit)
				sFilterShow.insert(c.path);
			return hit || childHit;
		};
		for (Category& c : tree)
			mark(c);
	}

	const float listH = std::max(180.f, ImGui::GetContentRegionAvail().y - 48.f);
	ImGui::BeginChild("##tekkit_cats", ImVec2(0.f, listH), true);

	if (filterOn)
	{
		/* Flat filtered results - checkboxes, no tree. */
		std::function<void(Category&)> flat = [&](Category& c) {
			if (c.hidden || c.separator)
				return;
			if (sFilterShow.find(c.path) == sFilterShow.end())
				return;
			ImGui::PushID(c.path.c_str());
			bool en = c.enabled;
			if (ImGui::Checkbox(c.label.c_str(), &en))
			{
				SetCategoryEnabled(c.path, en);
				dirty = true;
			}
			if (ImGui::IsItemHovered())
			{
				if (!c.tip.empty())
					ImGui::SetTooltip("%s\n\n%s", c.tip.c_str(), c.path.c_str());
				else
					ImGui::SetTooltip("%s", c.path.c_str());
			}
			ImGui::PopID();
			for (Category& ch : c.children)
				flat(ch);
		};
		for (Category& c : tree)
			flat(c);
	}
	else
	{
		for (Category& c : *level)
		{
			if (c.hidden)
				continue;
			if (c.separator)
			{
				ImGui::Spacing();
				ImGui::TextDisabled("%s", c.label.c_str());
				continue;
			}
			ImGui::PushID(c.path.c_str());
			bool en = c.enabled;
			const bool hasKids = !c.children.empty();
			const float openW = hasKids ? (PadNav::ButtonWidth("Open") + 8.f) : 0.f;
			const float rowStartX = ImGui::GetCursorPosX();
			const float rowAvail = ImGui::GetContentRegionAvail().x;
			/* Same-line Open only when the label still has room to breathe. */
			const bool openInline = hasKids && (rowAvail >= openW + 120.f);

			if (ImGui::Checkbox("##en", &en))
			{
				SetCategoryEnabled(c.path, en);
				dirty = true;
			}
			ImGui::SameLine(0.f, 6.f);
			ImGui::AlignTextToFramePadding();
			const float afterCheck = ImGui::GetCursorPosX();
			const float labMax = std::max(48.f,
				rowAvail - (afterCheck - rowStartX) - (openInline ? openW : 0.f));
			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + labMax);
			ImGui::TextUnformatted(c.label.c_str());
			ImGui::PopTextWrapPos();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
			{
				if (!c.tip.empty())
					ImGui::SetTooltip("%s\n\n%s", c.tip.c_str(), c.path.c_str());
				else
					ImGui::SetTooltip("%s", c.path.c_str());
			}
			if (hasKids)
			{
				const bool singleLine =
					ImGui::GetItemRectSize().y <= ImGui::GetTextLineHeightWithSpacing() * 1.15f;
				if (openInline && singleLine)
				{
					ImGui::SameLine(0.f, 0.f);
					ImGui::SetCursorPosX(rowStartX + rowAvail - openW);
				}
				if (ImGui::SmallButton("Open"))
					sDrill.push_back(c.path);
			}
			ImGui::PopID();
		}
	}
	ImGui::EndChild();

	if (!loading && TrailCount() == 0 && MarkerCount() == 0)
	{
		PadNav::PushWrap();
		ImGui::TextColored(ImVec4(1.f, 0.75f, 0.35f, 1.f),
			"Nothing visible on this map - enable categories above or in Features.");
		PadNav::PopWrap();
	}
	else
	{
		PadNav::PushWrap();
		ImGui::TextColored(HelperTheme::Muted,
			"Enabled categories draw on compass + world GPS.");
		PadNav::PopWrap();
	}

	return dirty;
}

bool PathingTrails::DrawSettings()
{
	bool dirty = false;
	dirty |= DrawOverlaySettings();
	ImGui::Separator();
	dirty |= DrawPackTools();
	ImGui::Separator();
	dirty |= DrawCategoryBrowser();
	return dirty;
}
