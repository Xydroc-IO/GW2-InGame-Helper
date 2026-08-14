#include "CompletionShared.h"

#include "BrowserTabs.h"
#include "Globals.h"
#include "Gw2Icons.h"
#include "Gw2Ui.h"
#include "HelperTheme.h"
#include "PadLayout.h"
#include "PadNav.h"
#include "Settings.h"
#include "WikiBrowser.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace CompletionDetail
{
	namespace
	{
		std::string ToLowerCopy(std::string s)
		{
			for (char& c : s)
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return s;
		}

		void CatProgress(const AchCategory& c, int& done, int& total)
		{
			done = 0;
			total = static_cast<int>(c.achievementIds.size());
			for (int id : c.achievementIds)
			{
				ApProgress p{};
				if (LookupApProgress(static_cast<uint32_t>(id), p) && p.done)
					++done;
			}
		}

		void OpenWiki(const char* name)
		{
			if (!name || !name[0])
				return;
			const std::string enc = WikiBrowser::UrlEncode(name);
			char url[384];
			std::snprintf(url, sizeof(url),
				"https://wiki.guildwars2.com/wiki/Special:Search?search=%s&go=Go", enc.c_str());
			G::ShowWiki = true;
			Settings::SetDirty();
			if (BrowserTabs::OpenNewUrl("wiki", url) < 0)
				WikiBrowser::Navigate(url);
		}

		bool BitDone(const ApProgress& p, int index)
		{
			if (p.done)
				return true;
			if (!p.bits.empty())
			{
				for (int b : p.bits)
				{
					if (b == index)
						return true;
				}
				return false;
			}
			return false;
		}

		ImU32 ColU32(const ImVec4& c)
		{
			return ImGui::ColorConvertFloat4ToU32(c);
		}

		void DrawOpenMark(float size)
		{
			const ImVec2 p0 = ImGui::GetCursorScreenPos();
			ImGui::Dummy(ImVec2(size, size));
			ImDrawList* dl = ImGui::GetWindowDrawList();
			const float pad = 2.f;
			dl->AddRect(
				ImVec2(p0.x + pad, p0.y + pad),
				ImVec2(p0.x + size - pad, p0.y + size - pad),
				ColU32(HelperTheme::Muted), 2.f, 0, 1.5f);
		}

		void BitLabel(const AchBit& b, int index, char* out, size_t outLen)
		{
			if (!out || outLen == 0)
				return;
			out[0] = '\0';
			if (!b.text.empty())
			{
				std::snprintf(out, outLen, "%s", b.text.c_str());
				return;
			}
			if (b.kind == AchBitKind::Achievement && b.targetId > 0)
			{
				const AchDef* child = FindAchDef(b.targetId);
				if (child && !child->name.empty())
				{
					std::snprintf(out, outLen, "%s", child->name.c_str());
					return;
				}
				std::snprintf(out, outLen, "Achievement #%d", b.targetId);
				return;
			}
			if (b.kind == AchBitKind::Item && b.targetId > 0 &&
				Gw2Icons::ItemName(b.targetId, out, outLen))
				return;
			if (b.kind == AchBitKind::Mini && b.targetId > 0 &&
				Gw2Icons::MiniName(b.targetId, out, outLen))
				return;
			if (b.kind == AchBitKind::Skin && b.targetId > 0 &&
				Gw2Icons::SkinName(b.targetId, out, outLen))
				return;
			if (b.targetId > 0)
				std::snprintf(out, outLen, "%s %d",
					b.kind == AchBitKind::Skin ? "Skin" :
					b.kind == AchBitKind::Mini ? "Mini" : "Item",
					b.targetId);
			else
				std::snprintf(out, outLen, "Objective %d", index + 1);
		}

		void EnsureBitNames(const AchDef& d)
		{
			std::vector<int> missing;
			for (const AchBit& b : d.bits)
			{
				if (b.targetId <= 0)
					continue;
				if (b.kind == AchBitKind::Item)
					Gw2Icons::RequestItem(b.targetId);
				else if (b.kind == AchBitKind::Mini)
					Gw2Icons::RequestMini(b.targetId);
				else if (b.kind == AchBitKind::Skin)
					Gw2Icons::RequestSkin(b.targetId);
				else if (b.kind == AchBitKind::Achievement && !FindAchDef(b.targetId))
					missing.push_back(b.targetId);
			}
			if (!missing.empty() && !AchDefsBusy())
				BeginAchDefsForIds(missing);
		}

		bool DrawBitIcon(const AchBit& b, float size)
		{
			if (b.targetId <= 0)
				return false;
			if (b.kind == AchBitKind::Item)
				return Gw2Icons::ImageItem(b.targetId, size);
			if (b.kind == AchBitKind::Mini)
				return Gw2Icons::ImageMini(b.targetId, size);
			if (b.kind == AchBitKind::Skin)
				return Gw2Icons::ImageSkin(b.targetId, size);
			return false;
		}

		bool BitsLookLikeCollection(const std::vector<AchBit>& bits)
		{
			int withId = 0;
			int namedSlots = 0;
			int achBits = 0;
			for (const AchBit& b : bits)
			{
				if (b.kind == AchBitKind::Achievement)
					++achBits;
				if (b.targetId > 0 && (b.kind == AchBitKind::Item ||
					b.kind == AchBitKind::Skin || b.kind == AchBitKind::Mini))
					++withId;
				if (b.text.rfind("Tier ", 0) == 0)
					++namedSlots;
			}
			const int n = static_cast<int>(bits.size());
			if (achBits * 2 >= n && achBits >= 4)
				return false;
			if (namedSlots >= 12 && namedSlots * 2 >= n)
				return true;
			return withId >= 4 && withId * 2 >= n;
		}

		void DrawBitGrid(const std::vector<AchBit>& bits, const ApProgress& p, bool known)
		{
			const int n = static_cast<int>(bits.size());
			int cols = 6;
			if (n <= 4)
				cols = n;
			else if (n % 5 == 0 && n % 6 != 0)
				cols = 5;
			else if (n <= 8)
				cols = 4;
			if (cols < 1)
				cols = 1;
			const float sz = 40.f;
			const float gap = 5.f;
			const float cell = sz + gap;
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImGui::PushTextWrapPos(-1.f);
			const ImVec2 origin = ImGui::GetCursorScreenPos();
			for (int i = 0; i < n; ++i)
			{
				const int col = i % cols;
				const int row = i / cols;
				ImGui::SetCursorScreenPos(ImVec2(origin.x + col * cell, origin.y + row * cell));
				const AchBit& b = bits[static_cast<size_t>(i)];
				const bool on = known && BitDone(p, i);
				ImGui::PushID(i);
				const ImVec2 p0 = ImGui::GetCursorScreenPos();
				ImGui::InvisibleButton("###gw2igh_ap_bit", ImVec2(sz, sz));
				const ImVec2 p1(p0.x + sz, p0.y + sz);
				dl->AddRectFilled(p0, p1, ColU32(HelperTheme::Child), 3.f);
				dl->AddRect(p0, p1, ColU32(on ? HelperTheme::Ok : HelperTheme::Border), 3.f);
				ImGui::SetCursorScreenPos(ImVec2(p0.x + 2.f, p0.y + 2.f));
				{
					const bool iconKind = b.kind == AchBitKind::Item ||
						b.kind == AchBitKind::Mini || b.kind == AchBitKind::Skin;
					bool drew = false;
					if (iconKind)
					{
						if (!on)
							ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.42f);
						drew = DrawBitIcon(b, sz - 4.f);
						if (!on)
							ImGui::PopStyleVar();
					}
					if (!drew)
					{
						if (iconKind)
							ImGui::SetCursorScreenPos(ImVec2(p0.x + 2.f, p0.y + 2.f));
						char lab[160];
						BitLabel(b, i, lab, sizeof(lab));
						ImGui::PushTextWrapPos(p0.x + sz - 2.f);
						ImGui::PushStyleColor(ImGuiCol_Text, on ? HelperTheme::Ok : HelperTheme::Muted);
						ImGui::TextUnformatted(lab);
						ImGui::PopStyleColor();
						ImGui::PopTextWrapPos();
					}
				}
				if (on)
				{
					ImGui::SetCursorScreenPos(ImVec2(p1.x - 16.f, p1.y - 16.f));
					Gw2Ui::Image(Gw2Ui::Icon::Tick, 14.f);
				}
				if (ImGui::IsMouseHoveringRect(p0, p1))
				{
					char lab[160];
					BitLabel(b, i, lab, sizeof(lab));
					ImGui::SetTooltip("%s%s", lab, on ? "\nComplete" : "\nMissing");
				}
				ImGui::PopID();
			}
			const int rows = n <= 0 ? 0 : (n + cols - 1) / cols;
			ImGui::SetCursorScreenPos(origin);
			if (rows > 0)
				ImGui::Dummy(ImVec2(cols * cell - gap, rows * cell - gap));
			ImGui::PopTextWrapPos();
		}

		void DrawDoneMark(bool on, float size)
		{
			if (on)
			{
				if (!Gw2Ui::Image(Gw2Ui::Icon::Tick, size))
					ImGui::Dummy(ImVec2(size, size));
			}
			else
				DrawOpenMark(size);
		}

		void DrawBitList(const std::vector<AchBit>& bits, const ApProgress& p, bool known)
		{
			for (size_t i = 0; i < bits.size(); ++i)
			{
				const AchBit& b = bits[i];
				const bool on = known && BitDone(p, static_cast<int>(i));
				ImGui::PushID(static_cast<int>(i));
				const float xLine = ImGui::GetCursorPosX();
				const float yLine = ImGui::GetCursorPosY();
				constexpr float kMark = 16.f;
				constexpr float kIcon = 22.f;
				DrawDoneMark(on, kMark);
				ImGui::SameLine(0.f, 6.f);
				float rowH = (std::max)(kMark, ImGui::GetTextLineHeightWithSpacing());
				if (b.kind == AchBitKind::Item || b.kind == AchBitKind::Mini ||
					b.kind == AchBitKind::Skin)
				{
					if (!DrawBitIcon(b, kIcon))
						ImGui::Dummy(ImVec2(kIcon, kIcon));
					ImGui::SameLine(0.f, 6.f);
					rowH = (std::max)(rowH, kIcon);
				}
				char lab[192];
				BitLabel(b, static_cast<int>(i), lab, sizeof(lab));
				ImGui::PushTextWrapPos(-1.f);
				if (b.kind == AchBitKind::Achievement && b.targetId > 0)
				{
					if (on)
						ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::Ok);
					if (ImGui::Selectable(lab, false))
						gApSelAchId = b.targetId;
					if (on)
						ImGui::PopStyleColor();
				}
				else if (on)
					ImGui::TextColored(HelperTheme::Ok, "%s", lab);
				else
					ImGui::TextColored(HelperTheme::Muted, "%s", lab);
				ImGui::PopTextWrapPos();
				ImGui::SetCursorPos(ImVec2(xLine, yLine + rowH));
				ImGui::PopID();
			}
		}

		void DrawTierList(const std::vector<AchTier>& tiers, const ApProgress& p, bool known, bool done)
		{
			for (size_t i = 0; i < tiers.size(); ++i)
			{
				const AchTier& t = tiers[i];
				const bool on = done || (known && p.current >= t.count);
				ImGui::PushID(static_cast<int>(i) + 70000);
				const float xLine = ImGui::GetCursorPosX();
				const float yLine = ImGui::GetCursorPosY();
				DrawDoneMark(on, 16.f);
				ImGui::SameLine(0.f, 6.f);
				if (on)
					ImGui::TextColored(HelperTheme::Ok, "Reach %d  ·  %d AP", t.count, t.points);
				else
					ImGui::TextColored(HelperTheme::Muted, "Reach %d  ·  %d AP", t.count, t.points);
				const float rowH = (std::max)(16.f, ImGui::GetTextLineHeightWithSpacing());
				ImGui::SetCursorPos(ImVec2(xLine, yLine + rowH));
				ImGui::PopID();
			}
		}

		void DrawAccountBitSlots(int maxSlots, const ApProgress& p, bool known)
		{
			if (maxSlots <= 0 || maxSlots > 80)
				return;
			for (int i = 0; i < maxSlots; ++i)
			{
				const bool on = known && BitDone(p, i);
				ImGui::PushID(i + 80000);
				const float xLine = ImGui::GetCursorPosX();
				const float yLine = ImGui::GetCursorPosY();
				DrawDoneMark(on, 16.f);
				ImGui::SameLine(0.f, 6.f);
				if (on)
					ImGui::TextColored(HelperTheme::Ok, "Objective %d", i + 1);
				else
					ImGui::TextColored(HelperTheme::Muted, "Objective %d  ·  missing", i + 1);
				const float rowH = (std::max)(16.f, ImGui::GetTextLineHeightWithSpacing());
				ImGui::SetCursorPos(ImVec2(xLine, yLine + rowH));
				ImGui::PopID();
			}
		}

		void DrawStatus(bool known, bool done, const ApProgress& p)
		{
			if (done)
				ImGui::TextColored(HelperTheme::Ok, "Done");
			else if (known && p.max > 0)
			{
				ImGui::TextColored(HelperTheme::Warn, "In progress  %d / %d", p.current, p.max);
				const float t = p.max > 0
					? static_cast<float>(p.current) / static_cast<float>(p.max) : 0.f;
				ImGui::ProgressBar(t, ImVec2(-1.f, 0.f), "");
			}
			else
				ImGui::TextColored(HelperTheme::Muted, "Not started on this account");
		}

		void DrawDetail(int id)
		{
			const AchDef* d = FindAchDef(id);
			ApProgress p{};
			const bool known = LookupApProgress(static_cast<uint32_t>(id), p);
			const bool done = known && p.done;
			PadNav::SectionTitle(d && !d->name.empty() ? d->name.c_str() : "Achievement");
			if (d)
				EnsureBitNames(*d);
			DrawStatus(known, done, p);
			if (d && d->points > 0)
				ImGui::TextColored(HelperTheme::GoldMuted, "%d AP", d->points);

			const char* how = nullptr;
			if (d && !d->requirement.empty())
				how = d->requirement.c_str();
			else if (d && !d->description.empty())
				how = d->description.c_str();
			PadNav::SectionTitle("What to do");
			PadNav::PushWrap();
			if (how)
				ImGui::TextUnformatted(how);
			else if (AchDefsBusy())
				ImGui::TextColored(HelperTheme::Muted, "Loading details...");
			else
				ImGui::TextColored(HelperTheme::Muted,
					"The API did not include a requirement for this one.");
			PadNav::PopWrap();

			const bool collection = d && BitsLookLikeCollection(d->bits);
			if (d && !d->name.empty() && !collection)
			{
				BeginAchWikiThumb(id, d->name.c_str());
				std::string thumb;
				if (LookupAchWikiThumbUrl(id, thumb))
					Gw2Icons::ImageUrl(thumb.c_str(), 120.f);
			}

			if (d && !d->description.empty() && d->description != d->requirement)
			{
				PadNav::SectionTitle("About");
				PadNav::PushWrap();
				ImGui::TextUnformatted(d->description.c_str());
				PadNav::PopWrap();
			}
			if (d && !d->lockedText.empty() && !done)
			{
				PadNav::SectionTitle("Locked");
				PadNav::PushWrap();
				ImGui::TextUnformatted(d->lockedText.c_str());
				PadNav::PopWrap();
			}
			if (d && !d->bits.empty())
			{
				const bool collection = BitsLookLikeCollection(d->bits);
				PadNav::SectionTitle(collection ? "Collection" : "Objectives");
				if (collection)
					DrawBitGrid(d->bits, p, known);
				else
					DrawBitList(d->bits, p, known);
			}
			else if (known && !p.bits.empty() && p.max > 1 && p.max <= 80)
			{
				PadNav::SectionTitle("Objectives");
				DrawAccountBitSlots(p.max, p, known);
			}
			else if (d && !d->tiers.empty())
			{
				PadNav::SectionTitle("Tiers");
				DrawTierList(d->tiers, p, known, done);
				if (d->bits.empty() && (p.bits.empty() || !known))
				{
					PadNav::PushWrap();
					ImGui::TextColored(HelperTheme::Muted,
						"This one is a count, not a checklist. ArenaNet does not name "
						"which finds are done — only %d of %d.",
						known ? p.current : 0, known && p.max > 0 ? p.max : d->tiers.back().count);
					PadNav::PopWrap();
				}
			}
			else if (known && p.max > 1)
			{
				PadNav::SectionTitle("Progress");
				PadNav::PushWrap();
				ImGui::TextColored(HelperTheme::Muted,
					"%d done, %d still needed. The API has no named objectives for this achievement.",
					p.current, p.max - p.current);
				PadNav::PopWrap();
			}
			if (PadLayout::GoldButton(IsAchPinned(id) ? "Unpin###gw2igh_ap_pin" : "Pin (working on)###gw2igh_ap_pin",
				!IsAchPinned(id), true))
			{
				if (!ToggleAchPin(id))
					std::snprintf(gStatus, sizeof(gStatus),
						"Working on is full (%d). Unpin one first.", kMaxAchPins);
			}
			if (d && !d->name.empty() &&
				PadLayout::GoldButton("Open Wiki###gw2igh_ap_wiki", false, false))
				OpenWiki(d->name.c_str());
		}
	}

	void DrawAchievementsTab()
	{
		ApplyApOverlayResult();
		ApplyAchCatalogResult();
		ApplyAchDefsResult();
		ApplyAchWikiThumbResult();
		LoadAchPins();
		BeginAchCatalogRefresh(false);
		if (!G::Gw2ApiKey[0])
			PadNav::Blurb("Add a GW2 API key with progression in Settings.");
		else
			PadNav::Blurb("Pick a category, then an achievement for what to do.");

		if (PadNav::RefreshButton("###gw2igh_ap_api"))
		{
			BeginApOverlayRefresh();
			BeginAchCatalogRefresh(true);
		}
		ImGui::SameLine();
		if (ApOverlayBusy() || AchCatalogBusy())
			PadNav::StatusBusy("Loading...");
		else
			ImGui::TextColored(HelperTheme::Muted, "%d on account",
				static_cast<int>(ApProgressCount()));

		if (AchCatalogBusy() && !AchCatalogReady())
			return;
		if (!AchCatalogReady())
		{
			ImGui::TextColored(HelperTheme::Warn, "Could not load achievement groups.");
			return;
		}

		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::InputTextWithHint("###gw2igh_ap_q", "Search this category",
			gApSearch, sizeof(gApSearch));
		if (ImGui::SmallButton("All###gw2igh_ap_fa"))
			gApFilter = 0;
		ImGui::SameLine();
		if (ImGui::SmallButton("Done###gw2igh_ap_fd"))
			gApFilter = 1;
		ImGui::SameLine();
		if (ImGui::SmallButton("Open###gw2igh_ap_fo"))
			gApFilter = 2;

		PadNav::SectionTitle("Working on");
		{
			const std::vector<int>& pins = AchPins();
			if (pins.empty())
				ImGui::TextColored(HelperTheme::Muted, "Pin up to %d from the detail below.",
					kMaxAchPins);
			else
			{
				for (int pid : pins)
				{
					ImGui::PushID(pid + 900000);
					const AchDef* pd = FindAchDef(pid);
					char lab[160];
					if (pd && !pd->name.empty())
						std::snprintf(lab, sizeof(lab), "%s###gw2igh_ap_pw", pd->name.c_str());
					else
						std::snprintf(lab, sizeof(lab), "#%d###gw2igh_ap_pw", pid);
					if (ImGui::SmallButton(lab))
						FocusAchPin(pid);
					ImGui::SameLine();
					if (ImGui::SmallButton("x###gw2igh_ap_px"))
						ToggleAchPin(pid);
					ImGui::PopID();
				}
			}
		}

		static int sDefsFor = -1;
		if (gApSelCatId > 0 && sDefsFor != gApSelCatId && !AchDefsBusy())
		{
			sDefsFor = gApSelCatId;
			BeginAchDefsRefresh(gApSelCatId);
		}

		ImGui::NewLine();
		const float splitAvail = ImGui::GetContentRegionAvail().x;
		const float splitGap = ImGui::GetStyle().ItemSpacing.x;
		const float treeW = splitAvail * 0.42f;
		const float listW = splitAvail - treeW - splitGap;
		const float splitH = ImGui::GetContentRegionAvail().y * 0.42f;
		ImGui::BeginChild("###gw2igh_ap_acctree", ImVec2(treeW, splitH), true);
		for (const AchGroup& g : AchGroups())
		{
			ImGui::PushID(g.id.c_str());
			if (ImGui::TreeNodeEx("##g", ImGuiTreeNodeFlags_SpanAvailWidth, "%s",
				g.name.c_str()))
			{
				for (int cid : g.categoryIds)
				{
					const AchCategory* c = FindAchCategory(cid);
					if (!c)
						continue;
					int done = 0, total = 0;
					CatProgress(*c, done, total);
					char lab[160];
					std::snprintf(lab, sizeof(lab), "%s  %d/%d",
						c->name.empty() ? "?" : c->name.c_str(), done, total);
					if (ImGui::Selectable(lab, gApSelCatId == c->id))
					{
						gApSelCatId = c->id;
						gApSelAchId = 0;
					}
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
		ImGui::SameLine(0.f, splitGap);
		ImGui::BeginChild("###gw2igh_ap_acclist", ImVec2(listW, splitH), true);
		const AchCategory* cat = FindAchCategory(gApSelCatId);
		if (!cat)
			ImGui::TextColored(HelperTheme::Muted, "Pick a category.");
		else
		{
			if (AchDefsBusy())
				PadNav::StatusBusy("Loading names...");
			const std::string q = ToLowerCopy(gApSearch);
			int shown = 0;
			int firstId = 0;
			for (int id : cat->achievementIds)
			{
				ApProgress p{};
				const bool known = LookupApProgress(static_cast<uint32_t>(id), p);
				const bool done = known && p.done;
				if (gApFilter == 1 && !done)
					continue;
				if (gApFilter == 2 && done)
					continue;
				const AchDef* d = FindAchDef(id);
				const char* nm = (d && !d->name.empty()) ? d->name.c_str() : nullptr;
				char fallback[32];
				if (!nm)
				{
					std::snprintf(fallback, sizeof(fallback), "#%d", id);
					nm = fallback;
				}
				if (!q.empty() && ToLowerCopy(nm).find(q) == std::string::npos)
					continue;
				if (firstId == 0)
					firstId = id;
				++shown;
				ImGui::PushID(id);
				char row[192];
				if (done)
					std::snprintf(row, sizeof(row), "%s", nm);
				else if (known && p.max > 0)
					std::snprintf(row, sizeof(row), "%s  %d/%d", nm, p.current, p.max);
				else
					std::snprintf(row, sizeof(row), "%s", nm);
				if (done)
					ImGui::PushStyleColor(ImGuiCol_Text, HelperTheme::Ok);
				if (ImGui::Selectable(row, gApSelAchId == id))
					gApSelAchId = id;
				if (done)
					ImGui::PopStyleColor();
				ImGui::PopID();
			}
			if (gApSelAchId == 0 && firstId != 0)
				gApSelAchId = firstId;
			if (shown == 0)
				ImGui::TextColored(HelperTheme::Muted, "Nothing in this filter.");
		}
		ImGui::EndChild();

		ImGui::BeginChild("###gw2igh_ap_detail", ImVec2(0.f, 0.f), true,
			ImGuiWindowFlags_AlwaysVerticalScrollbar);
		if (gApSelAchId > 0)
			DrawDetail(gApSelAchId);
		else
			ImGui::TextColored(HelperTheme::Muted,
				"Click an achievement above to see how to complete it.");
		ImGui::EndChild();
	}
}
