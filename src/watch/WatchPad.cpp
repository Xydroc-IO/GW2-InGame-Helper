#include "WatchPad.h"

#include "EiRuntime.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PadDock.h"
#include "PadNav.h"
#include "Settings.h"
#include "WatchCapture.h"
#include "WatchLinux.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <d3d11.h>

namespace
{
	bool gRequestDock = false;
	bool gRequestMirrorDock = false;
	int  gSelected = -1;
	char gFilter[64] = {};

	void EnsureList()
	{
		if (EiRuntime::IsWine())
			return;
		if (WatchCapture::Windows().empty())
			WatchCapture::RefreshWindowList();
	}

	ImU32 ColU32(const ImVec4& c, float aMul = 1.f)
	{
		ImVec4 t = c;
		t.w *= aMul;
		return ImGui::ColorConvertFloat4ToU32(t);
	}

	void PaintStageFrame(ImDrawList* dl, ImVec2 p0, ImVec2 p1, bool live)
	{
		if (!dl)
			return;
		const float a = G::Opacity > 0.15f ? G::Opacity : 0.97f;
		dl->AddRectFilled(p0, p1, IM_COL32(4, 3, 2, static_cast<int>(235 * a + 0.5f)));
		dl->AddRect(p0, p1, ColU32(HelperTheme::GoldDim, 0.85f * a), 0.f, 0, 1.6f);
		const ImVec2 i0(p0.x + 3.f, p0.y + 3.f);
		const ImVec2 i1(p1.x - 3.f, p1.y - 3.f);
		if (i1.x > i0.x && i1.y > i0.y)
			dl->AddRect(i0, i1, ColU32(HelperTheme::GoldBright, 0.55f * a), 0.f, 0, 1.f);

		const float tick = 10.f;
		const ImU32 tickCol = ColU32(HelperTheme::Gold, 0.9f * a);
		auto corner = [&](float x, float y, float dx, float dy) {
			dl->AddLine(ImVec2(x, y), ImVec2(x + dx * tick, y), tickCol, 1.8f);
			dl->AddLine(ImVec2(x, y), ImVec2(x, y + dy * tick), tickCol, 1.8f);
		};
		corner(p0.x + 1.f, p0.y + 1.f, 1.f, 1.f);
		corner(p1.x - 1.f, p0.y + 1.f, -1.f, 1.f);
		corner(p0.x + 1.f, p1.y - 1.f, 1.f, -1.f);
		corner(p1.x - 1.f, p1.y - 1.f, -1.f, -1.f);

		if (live)
		{
			const float r = 4.f;
			const ImVec2 c(p0.x + 14.f, p0.y + 12.f);
			dl->AddCircleFilled(c, r, ColU32(HelperTheme::Ok, a));
			dl->AddCircle(c, r + 1.5f, ColU32(HelperTheme::GoldBright, 0.7f * a), 0, 1.2f);
		}
	}

	void OpenMirror()
	{
		if (!G::ShowWatchMirror)
		{
			G::ShowWatchMirror = true;
			gRequestMirrorDock = true;
			Settings::SetDirty();
		}
	}

	bool RenderMirror()
	{
		if (!G::ShowWatchMirror)
			return false;

		constexpr float kPadW = 960.f;
		constexpr float kPadH = 540.f;
		PadDock::SetSizeConstraints("Watch Mirror###GW2InGameHelperWatchMirror", 480.f, 300.f,
			PadDock::MaxW(1600.f), PadDock::MaxH(1000.f));
		ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
		PadDock::Place(G::PadWatchMirror, gRequestMirrorDock, kPadW, kPadH,
			PadDock::BesideHelper(kPadW));
		if (!gRequestMirrorDock && G::PadWatchMirror.w < 80.f)
			ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);

		bool open = G::ShowWatchMirror;
		HelperTheme::ScopedWindow theme(G::Opacity);
		const bool padBody = ImGui::Begin("Watch Mirror###GW2InGameHelperWatchMirror", &open,
			HelperTheme::PadFlags());
		if (!theme.AfterBegin("Watch Mirror", &open) || !padBody)
		{
			if (PadDock::Capture(G::PadWatchMirror))
				Settings::SetDirty();
			const bool hovered = ImGui::IsWindowHovered(
				ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
				ImGuiHoveredFlags_ChildWindows);
			HelperTheme::EndPad();
			if (!open)
			{
				G::ShowWatchMirror = false;
				WatchCapture::Stop();
				Settings::SetDirty();
			}
			return hovered;
		}

		HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);
		ImGui::PushID("gw2igh_watch_mirror");

		const bool capturing = WatchCapture::IsCapturing();
		const bool wine = EiRuntime::IsWine();
		constexpr float kPad = 6.f;
		constexpr float kInset = 8.f;
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		const float stageW = (std::max)(40.f, avail.x);
		const float stageH = (std::max)(96.f, avail.y - 2.f);
		const ImVec2 stageCursor = ImGui::GetCursorScreenPos();
		ImGui::Dummy(ImVec2(stageW, stageH));
		const ImVec2 stage0 = stageCursor;
		const ImVec2 stage1(stageCursor.x + stageW, stageCursor.y + stageH);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		PaintStageFrame(dl, stage0, stage1, capturing);

		const uint32_t cw = WatchCapture::ContentW();
		const uint32_t ch = WatchCapture::ContentH();
		ID3D11ShaderResourceView* srv = WatchCapture::Srv();
		const ImVec2 inner0(stage0.x + kPad, stage0.y + kPad);
		const ImVec2 inner1(stage1.x - kPad, stage1.y - kPad);
		const float innerW = inner1.x - inner0.x;
		const float innerH = inner1.y - inner0.y;
		if (srv && cw > 0 && ch > 0 && innerW > 8.f && innerH > 8.f)
		{
			const float aspect = static_cast<float>(cw) / static_cast<float>(ch);
			float drawW = innerW;
			float drawH = drawW / aspect;
			if (drawH > innerH)
			{
				drawH = innerH;
				drawW = drawH * aspect;
			}
			const float ox = (innerW - drawW) * 0.5f;
			const float oy = (innerH - drawH) * 0.5f;
			const ImVec2 img0(inner0.x + ox, inner0.y + oy);
			const ImVec2 img1(img0.x + drawW, img0.y + drawH);
			dl->AddImage(reinterpret_cast<ImTextureID>(srv), img0, img1);
			dl->AddRect(img0, img1, ColU32(HelperTheme::GoldDim, 0.35f * G::Opacity), 0.f, 0, 1.f);
		}
		else
		{
			ImGui::SetCursorScreenPos(ImVec2(inner0.x + kInset, inner0.y + kInset));
			ImGui::PushTextWrapPos(inner1.x - kInset);
			ImGui::TextColored(HelperTheme::Muted, "%s",
				capturing
					? (wine
						? "Waiting for portal / first frame…"
						: "Waiting for first frame…")
					: "Stopped.");
			ImGui::PopTextWrapPos();
		}

		ImGui::PopID();
		if (PadDock::Capture(G::PadWatchMirror))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
			ImGuiHoveredFlags_ChildWindows);
		HelperTheme::EndPad();
		if (!open)
		{
			G::ShowWatchMirror = false;
			WatchCapture::Stop();
			Settings::SetDirty();
		}
		return hovered;
	}
}

void WatchPad::Open()
{
	G::ShowWatch = true;
	gRequestDock = true;
	WatchCapture::RefreshWindowList();
	gSelected = -1;
	if (WatchLinux::Available())
		WatchLinux::WarmAsync();
	Settings::SetDirty();
}

void WatchPad::CloseAll()
{
	G::ShowWatch = false;
	G::ShowWatchMirror = false;
	WatchCapture::Stop();
	Settings::SetDirty();
}

bool WatchPad::Render()
{
	bool hovered = false;

	/* One present/tick for both windows. */
	if (G::ShowWatch || G::ShowWatchMirror)
		WatchCapture::Tick();

	/* Auto-pop mirror once capture is live (portal/window chosen). */
	if (WatchCapture::IsCapturing() && !G::ShowWatchMirror)
		OpenMirror();

	if (G::ShowWatch)
	{
		constexpr float kPadW = 440.f;
		constexpr float kPadH = 260.f;

		PadDock::SetSizeConstraints("Watch###GW2InGameHelperWatch", 320.f, 180.f,
			PadDock::MaxW(640.f), PadDock::MaxH(520.f));
		ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
		PadDock::Place(G::PadWatch, gRequestDock, kPadW, kPadH, PadDock::BesideHelper(kPadW));
		if (!gRequestDock && G::PadWatch.w < 80.f)
			ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);

		bool open = G::ShowWatch;
		HelperTheme::ScopedWindow theme(G::Opacity);
		const bool padBody = ImGui::Begin("Watch###GW2InGameHelperWatch", &open, HelperTheme::PadFlags());
		if (!theme.AfterBegin("Watch", &open) || !padBody)
		{
			if (PadDock::Capture(G::PadWatch))
				Settings::SetDirty();
			hovered |= ImGui::IsWindowHovered(
				ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
				ImGuiHoveredFlags_ChildWindows);
			HelperTheme::EndPad();
			if (!open)
			{
				G::ShowWatch = false;
				Settings::SetDirty();
			}
		}
		else
		{
			HelperTheme::ScopedFontScale fontScale(kPadW, kPadH);
			ImGui::PushID("gw2igh_watch_pad");

			PadNav::Blurb(
				"Mirror a desktop window. Playback stays in that app — "
				"Helper only shows pixels. Look-only.");

			const bool wine = EiRuntime::IsWine();
			const bool capturing = WatchCapture::IsCapturing();
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
				EnsureList();
				const auto& wins = WatchCapture::Windows();

				ImGui::Spacing();
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
				ImGui::InputTextWithHint("##watch_filter", "Filter titles…", gFilter, sizeof(gFilter));
				ImGui::SameLine();
				if (ImGui::Button("Refresh"))
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

				ImGui::SetNextItemWidth(-1.f);
				const char* preview = wins.empty() ? "(no windows found)" : "(select a window)";
				if (gSelected >= 0 && gSelected < static_cast<int>(wins.size()))
					preview = wins[static_cast<size_t>(gSelected)].title.c_str();

				if (ImGui::BeginCombo("##watch_windows", preview))
				{
					if (wins.empty())
						ImGui::TextDisabled("Nothing to list — see status below.");
					for (int i = 0; i < static_cast<int>(wins.size()); ++i)
					{
						const auto& e = wins[static_cast<size_t>(i)];
						if (gFilter[0] && !std::strstr(e.title.c_str(), gFilter))
							continue;
						const bool sel = (i == gSelected);
						char label[160]{};
						std::snprintf(label, sizeof(label), "%s###id%llu", e.title.c_str(),
							static_cast<unsigned long long>(e.id));
						if (ImGui::Selectable(label, sel))
							gSelected = i;
						if (sel)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				const bool canStart = gSelected >= 0 && gSelected < static_cast<int>(wins.size())
					&& !capturing;
				if (canStart)
				{
					if (ImGui::Button("Start"))
					{
						WatchCapture::Start(wins[static_cast<size_t>(gSelected)].id);
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

			ImGui::Spacing();
			if (capturing)
				PadNav::StatusOk(WatchCapture::StatusText());
			else
				PadNav::Meta(WatchCapture::StatusText());
			if (WatchCapture::LastFrameLookedBlank())
			{
				PadNav::StatusWarn(
					"Black frames often mean DRM or a hardware overlay.");
			}

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

			ImGui::PopID();
			if (PadDock::Capture(G::PadWatch))
				Settings::SetDirty();
			hovered |= ImGui::IsWindowHovered(
				ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
				ImGuiHoveredFlags_ChildWindows);
			HelperTheme::EndPad();
			if (!open)
			{
				G::ShowWatch = false;
				Settings::SetDirty();
			}
		}
	}
	hovered |= RenderMirror();
	return hovered;
}
