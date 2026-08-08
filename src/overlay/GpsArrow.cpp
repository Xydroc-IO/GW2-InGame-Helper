#include "GpsArrow.h"

#include "CompletionShared.h"
#include "GameLive.h"
#include "Globals.h"
#include "HelperTheme.h"
#include "PathingTrails.h"

#include "imgui/imgui.h"

#include <cmath>
#include <cstdio>

bool GpsArrow::Render()
{
	if (!CompletionDetail::gShowGpsArrow)
		return false;
	if (!PathingTrails::HasSearchGuideActive())
		return false;
	if (!GameLive::IsLive())
		return false;
	if (!G::Mumble || G::Mumble->context_len < sizeof(MumbleContext))
		return false;

	float destX = 0.f, destY = 0.f;
	bool haveDest = false;
	if (CompletionDetail::gFocusObjective >= 0)
	{
		if (auto* o = CompletionDetail::ObjectiveAt(
				static_cast<size_t>(CompletionDetail::gFocusObjective)))
		{
			if (o->hasCoord)
			{
				destX = o->continentX;
				destY = o->continentY;
				haveDest = true;
			}
		}
	}
	if (!haveDest)
	{
		const PathingTrails::Trail guide = PathingTrails::SearchGuide();
		if (guide.points.size() >= 2)
		{
			destX = guide.points.back().x;
			destY = guide.points.back().y;
			haveDest = true;
		}
	}
	if (!haveDest)
		return false;

	const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
	const float dx = destX - ctx->playerX;
	const float dy = destY - ctx->playerY;
	const float dist = std::sqrt(dx * dx + dy * dy);
	if (dist < 1.f)
		return false;

	/* Bearing from continent delta; camera yaw from avatar front XZ. */
	const float targetYaw = std::atan2(dx, dy);
	const float fx = G::Mumble->fAvatarFront[0];
	const float fz = G::Mumble->fAvatarFront[2];
	const float camYaw = std::atan2(fx, fz);
	float rel = targetYaw - camYaw;
	while (rel > 3.14159265f) rel -= 6.2831853f;
	while (rel < -3.14159265f) rel += 6.2831853f;

	const ImGuiIO& io = ImGui::GetIO();
	const float size = 88.f * (G::FontScale > 0.2f ? G::FontScale : 1.f);
	ImGui::SetNextWindowPos(
		ImVec2(io.DisplaySize.x * 0.5f - size * 0.5f, io.DisplaySize.y * 0.18f),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(size + 24.f, size + 48.f), ImGuiCond_FirstUseEver);
	HelperTheme::ScopedOverlay theme(0.55f * G::Opacity);
	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoFocusOnAppearing;

	bool open = true;
	if (!ImGui::Begin("##gw2igh_gps_arrow", &open, flags))
	{
		ImGui::End();
		return false;
	}
	if (!open)
	{
		CompletionDetail::gShowGpsArrow = false;
		ImGui::End();
		return false;
	}

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 p0 = ImGui::GetCursorScreenPos();
	const ImVec2 c(p0.x + size * 0.5f + 4.f, p0.y + size * 0.5f + 4.f);
	const float r = size * 0.38f;
	const float cs = std::cos(rel);
	const float sn = std::sin(rel);
	const ImVec2 tip(c.x + sn * r, c.y - cs * r);
	const ImVec2 left(c.x + sn * (-r * 0.35f) - cs * (r * 0.45f),
		c.y - cs * (-r * 0.35f) - sn * (r * 0.45f));
	const ImVec2 right(c.x + sn * (-r * 0.35f) + cs * (r * 0.45f),
		c.y - cs * (-r * 0.35f) + sn * (r * 0.45f));
	const ImU32 gold = ImGui::ColorConvertFloat4ToU32(HelperTheme::Gold);
	const ImU32 rim = ImGui::ColorConvertFloat4ToU32(
		ImVec4(HelperTheme::Bg.x, HelperTheme::Bg.y, HelperTheme::Bg.z, 0.90f));
	dl->AddCircleFilled(c, r * 1.05f, rim, 32);
	dl->AddTriangleFilled(tip, left, right, gold);

	ImGui::Dummy(ImVec2(size + 8.f, size + 8.f));
	ImGui::TextColored(HelperTheme::Muted, "%.0f m", dist);
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		ImGui::OpenPopup("##gps_arr_ctx");
	if (ImGui::BeginPopup("##gps_arr_ctx"))
	{
		if (ImGui::MenuItem("Hide arrow"))
			CompletionDetail::gShowGpsArrow = false;
		if (ImGui::MenuItem("Clear GPS"))
			CompletionDetail::ClearGpsGuide();
		ImGui::EndPopup();
	}
	const bool hovered = ImGui::IsWindowHovered();
	ImGui::End();
	return hovered;
}
