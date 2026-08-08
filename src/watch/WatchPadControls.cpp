#include "WatchPadInternal.h"

#include "EiRuntime.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "Settings.h"
#include "WatchCapture.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace WatchPadDetail
{
	int  gSelected = -1;
	char gFilter[64] = {};

	void EnsureList()
	{
		if (EiRuntime::IsWine())
			return;
		/* Never auto-EnumWindows here — that runs during ImGui and has asserted on
		   native Windows after close/reopen. User hits Refresh (or we leave empty). */
	}

	void OpenMirror()
	{
		if (!G::ShowWatchMirror)
		{
			G::ShowWatchMirror = true;
			gRequestMirrorDock = true;
			if (G::PadWatchMirror.w < 80.f || G::PadWatchMirror.h < 80.f)
			{
				G::PadWatchMirror.w = 0.f;
				G::PadWatchMirror.h = 0.f;
			}
			if (ImGuiWindow* w = ImGui::FindWindowByName("Watch Mirror###GW2InGameHelperWatchMirror"))
				w->StateStorage.SetBool(w->GetID("##gw2igh_pad_collapsed"), false);
			Settings::SetDirty();
		}
	}

	void DrawWatchControls()
	{
		const bool wine = EiRuntime::IsWine();
		const bool capturing = WatchCapture::IsCapturing();

		PadNav::Blurb(
			"Mirror a desktop window. Playback stays in that app — Helper only shows pixels.");

		if (wine)
		{
			ImGui::Spacing();
			PadNav::Meta("Linux · portal + PipeWire. Start opens the share picker.");
			ImGui::Spacing();
			if (!capturing)
			{
				if (ImGui::Button("Start"))
				{
					WatchCapture::Start(0);
					OpenMirror();
				}
			}
			else
			{
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
				ImGui::Button("Start");
				ImGui::PopStyleVar();
			}
			ImGui::SameLine();
			if (capturing)
			{
				if (ImGui::Button("Stop"))
				{
					WatchCapture::Stop();
					G::ShowWatchMirror = false;
					Settings::SetDirty();
				}
			}
			else
			{
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
				ImGui::Button("Stop");
				ImGui::PopStyleVar();
			}
		}
		else
		{
			const bool wgc = WatchCapture::WgcAvailable();
			ImGui::Spacing();
			if (wgc && !WatchCapture::ClassicListMode())
			{
				PadNav::Meta("Windows · Start opens the system capture picker (thumbnails).");
				ImGui::Spacing();
				if (!capturing)
				{
					if (ImGui::Button("Start###watch_start"))
					{
						WatchCapture::SetClassicListMode(false);
						WatchCapture::StartWgcPicker();
						OpenMirror();
					}
				}
				else
				{
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
					ImGui::Button("Start###watch_start");
					ImGui::PopStyleVar();
				}
				ImGui::SameLine();
				if (capturing)
				{
					if (ImGui::Button("Stop###watch_stop"))
					{
						WatchCapture::Stop();
						G::ShowWatchMirror = false;
						Settings::SetDirty();
					}
				}
				else
				{
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
					ImGui::Button("Stop###watch_stop");
					ImGui::PopStyleVar();
				}
				ImGui::Spacing();
				if (ImGui::SmallButton("Classic list…###watch_classic"))
				{
					WatchCapture::SetClassicListMode(true);
					EnsureList();
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Fall back to titled HWND list + GDI capture.");
			}
			else
			{
				EnsureList();
				const auto& wins = WatchCapture::Windows();
				if (wgc)
				{
					PadNav::Meta("Classic list (GDI). Prefer system picker when you can.");
					if (ImGui::SmallButton("System picker…###watch_wgc"))
						WatchCapture::SetClassicListMode(false);
					ImGui::Spacing();
				}
				else
					PadNav::Meta("Windows · pick a titled app window, then Start.");

				ImGui::SetNextItemWidth((std::max)(40.f, ImGui::GetContentRegionAvail().x - 72.f));
				ImGui::InputTextWithHint("##watch_filter", "Filter titles…", gFilter, sizeof(gFilter));
				ImGui::SameLine();
				if (ImGui::Button("Refresh###watch_refresh"))
				{
					const uint64_t keep = (gSelected >= 0 && gSelected < static_cast<int>(wins.size()))
						? wins[static_cast<size_t>(gSelected)].id
						: 0;
					WatchCapture::RefreshWindowList();
					gSelected = -1;
					const auto& again = WatchCapture::Windows();
					if (keep)
					{
						for (int i = 0; i < static_cast<int>(again.size()); ++i)
						{
							if (again[static_cast<size_t>(i)].id == keep)
							{
								gSelected = i;
								break;
							}
						}
					}
				}

				const float listH = (std::max)(120.f, ImGui::GetContentRegionAvail().y * 0.42f);
				ImGui::BeginChild("###gw2igh_watch_winlist", ImVec2(0.f, listH), true);
				if (wins.empty())
					ImGui::TextDisabled("No windows listed — click Refresh after opening the player.");
				int shown = 0;
				for (int i = 0; i < static_cast<int>(wins.size()); ++i)
				{
					const auto& e = wins[static_cast<size_t>(i)];
					if (e.title.empty())
						continue;
					if (gFilter[0] && !std::strstr(e.title.c_str(), gFilter))
						continue;
					++shown;
					const bool sel = (i == gSelected);
					char safe[140]{};
					size_t o = 0;
					for (size_t t = 0; e.title[t] && o + 1 < sizeof(safe); ++t)
					{
						const char c = e.title[t];
						safe[o++] = (c == '#') ? '+' : c;
					}
					safe[o] = '\0';
					char label[168]{};
					std::snprintf(label, sizeof(label), "%s###wid%llu", safe,
						static_cast<unsigned long long>(e.id));
					if (ImGui::Selectable(label, sel, ImGuiSelectableFlags_AllowDoubleClick))
					{
						gSelected = i;
						if (ImGui::IsMouseDoubleClicked(0) && !capturing)
						{
							WatchCapture::Start(e.id);
							OpenMirror();
						}
					}
				}
				if (!wins.empty() && shown == 0)
					ImGui::TextDisabled("Filter hid every window.");
				ImGui::EndChild();

				const bool canStart = gSelected >= 0 && gSelected < static_cast<int>(wins.size())
					&& !capturing && !wins[static_cast<size_t>(gSelected)].title.empty();
				if (canStart)
				{
					if (ImGui::Button("Start###watch_start"))
					{
						WatchCapture::Start(wins[static_cast<size_t>(gSelected)].id);
						OpenMirror();
					}
				}
				else
				{
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
					ImGui::Button("Start###watch_start");
					ImGui::PopStyleVar();
					if (!capturing && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
						ImGui::SetTooltip("Select a window in the list first.");
				}
				ImGui::SameLine();
				if (capturing)
				{
					if (ImGui::Button("Stop###watch_stop"))
					{
						WatchCapture::Stop();
						G::ShowWatchMirror = false;
						Settings::SetDirty();
					}
				}
				else
				{
					ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
					ImGui::Button("Stop###watch_stop");
					ImGui::PopStyleVar();
				}
			}
		}

		ImGui::Spacing();
		if (capturing)
			PadNav::StatusOk(WatchCapture::StatusText());
		else
			PadNav::Meta(WatchCapture::StatusText());
		if (WatchCapture::LastFrameLookedBlank())
			PadNav::StatusWarn("Black frames often mean DRM or a hardware overlay.");

		if (ImGui::TreeNode("Crop chrome"))
		{
			auto cropSlider = [](const char* label, float* v) {
				float pct = *v * 100.f;
				if (ImGui::SliderFloat(label, &pct, 0.f, 45.f, "%.0f%%"))
				{
					*v = pct * 0.01f;
					Settings::SetDirty();
				}
			};
			cropSlider("Top", &G::WatchCropTop);
			cropSlider("Bottom", &G::WatchCropBottom);
			cropSlider("Left", &G::WatchCropLeft);
			cropSlider("Right", &G::WatchCropRight);
			if (ImGui::Button("Browser preset"))
			{
				G::WatchCropTop = 0.14f;
				G::WatchCropBottom = G::WatchCropLeft = G::WatchCropRight = 0.f;
				Settings::SetDirty();
			}
			ImGui::SameLine();
			if (ImGui::Button("None"))
			{
				G::WatchCropTop = G::WatchCropBottom = G::WatchCropLeft = G::WatchCropRight = 0.f;
				Settings::SetDirty();
			}
			ImGui::TreePop();
		}
	}
}
