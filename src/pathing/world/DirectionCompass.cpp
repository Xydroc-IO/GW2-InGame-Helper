#include "DirectionCompass.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "PadNav.h"
#include "PadDock.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <deque>

/* World N/E/S/W around the character (Raidcore-style). Gold theming is ours —
   no Raidcore source copied. Independent of Tekkit CompassOverlay. */

namespace
{
	constexpr float kPi = 3.14159265358979323846f;
	constexpr float kDeg = 180.f / kPi;
	constexpr float kNearClip = 0.5f;
	constexpr float kFarClip = 8000.f;
	constexpr float kDefaultFov = 1.222f;
	constexpr float kInchesToMeters = 0.0254f;
	bool gRequestDock = false;

	struct Vec3
	{
		float x = 0.f, y = 0.f, z = 0.f;
		Vec3() = default;
		Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
		float Dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
		Vec3 Cross(const Vec3& o) const
		{
			return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
		}
		float LengthSq() const { return x * x + y * y + z * z; }
		Vec3 Normalised() const
		{
			const float l = std::sqrt(LengthSq());
			return l > 1e-6f ? Vec3{x / l, y / l, z / l} : Vec3{};
		}
	};

	struct Mat4
	{
		float m[4][4]{};
		void Transform(float ix, float iy, float iz,
			float& ox, float& oy, float& oz, float& ow) const
		{
			ox = m[0][0] * ix + m[1][0] * iy + m[2][0] * iz + m[3][0];
			oy = m[0][1] * ix + m[1][1] * iy + m[2][1] * iz + m[3][1];
			oz = m[0][2] * ix + m[1][2] * iy + m[2][2] * iz + m[3][2];
			ow = m[0][3] * ix + m[1][3] * iy + m[2][3] * iz + m[3][3];
		}
		Mat4 operator*(const Mat4& b) const
		{
			Mat4 r{};
			for (int c = 0; c < 4; ++c)
				for (int row = 0; row < 4; ++row)
					for (int k = 0; k < 4; ++k)
						r.m[c][row] += m[k][row] * b.m[c][k];
			return r;
		}
	};

	bool Finite3(float x, float y, float z)
	{
		return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
	}

	bool ReasonablePos(float x, float y, float z)
	{
		return Finite3(x, y, z) &&
			std::fabs(x) < 1.0e6f && std::fabs(y) < 1.0e6f && std::fabs(z) < 1.0e6f;
	}

	float ParseFovRadians()
	{
		if (!G::Mumble)
			return kDefaultFov;
		char id[260]{};
		const wchar_t* w = G::Mumble->identity;
		size_t n = 0;
		for (; n < 255 && w[n]; ++n)
			id[n] = (w[n] < 128) ? static_cast<char>(w[n]) : ' ';
		id[n] = 0;
		const char* p = std::strstr(id, "\"fov\"");
		if (!p)
			p = std::strstr(id, "\"FOV\"");
		if (!p)
			return kDefaultFov;
		p = std::strchr(p, ':');
		if (!p)
			return kDefaultFov;
		const float fov = static_cast<float>(std::atof(p + 1));
		return (std::isfinite(fov) && fov > 0.2f && fov < 3.f) ? fov : kDefaultFov;
	}

	bool BuildViewProj(float screenW, float screenH, Mat4& out)
	{
		const float* cp = G::Mumble->fCameraPosition;
		const float* cf = G::Mumble->fCameraFront;
		const float* ct = G::Mumble->fCameraTop;
		if (!ReasonablePos(cp[0], cp[1], cp[2]))
			return false;
		if (!Finite3(cf[0], cf[1], cf[2]) || !Finite3(ct[0], ct[1], ct[2]))
			return false;

		Vec3 camPos{cp[0], cp[1], cp[2]};
		Vec3 f = Vec3{cf[0], cf[1], cf[2]}.Normalised();
		if (f.LengthSq() < 0.5f)
			return false;
		Vec3 topHint{ct[0], ct[1], ct[2]};
		Vec3 worldUp = (topHint.LengthSq() > 0.01f) ? topHint.Normalised()
			: Vec3{0.f, 1.f, 0.f};
		if (std::fabs(f.Dot(worldUp)) > 0.98f)
			worldUp = Vec3{0.f, 1.f, 0.f};
		Vec3 r = worldUp.Cross(f).Normalised();
		if (r.LengthSq() < 0.5f)
		{
			worldUp = Vec3{0.f, 1.f, 0.f};
			r = worldUp.Cross(f).Normalised();
		}
		if (r.LengthSq() < 0.5f)
			return false;
		Vec3 u = f.Cross(r).Normalised();
		if (u.LengthSq() < 0.5f)
			return false;

		Mat4 view{};
		view.m[0][0] = r.x; view.m[1][0] = r.y; view.m[2][0] = r.z;
		view.m[3][0] = -r.Dot(camPos);
		view.m[0][1] = u.x; view.m[1][1] = u.y; view.m[2][1] = u.z;
		view.m[3][1] = -u.Dot(camPos);
		view.m[0][2] = f.x; view.m[1][2] = f.y; view.m[2][2] = f.z;
		view.m[3][2] = -f.Dot(camPos);
		view.m[3][3] = 1.f;

		const float fov = ParseFovRadians();
		const float aspect = (screenH > 0.f) ? screenW / screenH : 1.7778f;
		const float tanHalfFov = std::tan(fov * 0.5f);
		if (!std::isfinite(tanHalfFov) || tanHalfFov < 1e-4f)
			return false;

		Mat4 proj{};
		proj.m[0][0] = 1.f / (aspect * tanHalfFov);
		proj.m[1][1] = 1.f / tanHalfFov;
		proj.m[2][2] = kFarClip / (kFarClip - kNearClip);
		proj.m[2][3] = 1.f;
		proj.m[3][2] = -(kNearClip * kFarClip) / (kFarClip - kNearClip);

		out = proj * view;
		return true;
	}

	bool WorldToScreen(const Vec3& world, const Mat4& viewProj,
		float screenW, float screenH, float& sx, float& sy)
	{
		if (!ReasonablePos(world.x, world.y, world.z))
			return false;
		float cx, cy, cz, cw;
		viewProj.Transform(world.x, world.y, world.z, cx, cy, cz, cw);
		if (!std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(cw) || cw <= 0.05f)
			return false;
		const float ndcX = cx / cw;
		const float ndcY = cy / cw;
		if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
			return false;
		if (ndcX < -1.25f || ndcX > 1.25f || ndcY < -1.25f || ndcY > 1.25f)
			return false;
		sx = (ndcX + 1.f) * 0.5f * screenW;
		sy = (-ndcY + 1.f) * 0.5f * screenH;
		return std::isfinite(sx) && std::isfinite(sy);
	}

	float CameraYawDegrees()
	{
		const float fx = G::Mumble->fCameraFront[0];
		const float fz = G::Mumble->fCameraFront[2];
		if (!std::isfinite(fx) || !std::isfinite(fz))
			return 0.f;
		return std::atan2(fx, fz) * kDeg;
	}

	float Wrap360(float deg)
	{
		while (deg < 0.f)
			deg += 360.f;
		while (deg >= 360.f)
			deg -= 360.f;
		return deg;
	}

	float AngleDeltaAbs(float a, float b)
	{
		float d = std::fabs(Wrap360(a) - Wrap360(b));
		if (d > 180.f)
			d = 360.f - d;
		return d;
	}

	/* Read Nexus FontBig only — never PushFont (shared ImGui stack). */
	ImFont* DirectionFontBig()
	{
		if (G::NexusLink && G::NexusLink->FontBig)
			return static_cast<ImFont*>(G::NexusLink->FontBig);
		return ImGui::GetFont();
	}

	float LetterPixelSize(ImFont* font)
	{
		if (!font)
			return ImGui::GetFontSize();
		const float sz = font->FontSize * (font->Scale > 0.f ? font->Scale : 1.f);
		const float base = (sz > 0.f) ? sz : ImGui::GetFontSize();
		return base * std::clamp(G::DirectionLetterScale, 0.5f, 2.5f);
	}

	void DrawOutlinedText(ImDrawList* dl, ImFont* font, float sz, ImVec2 p,
		ImU32 fill, ImU32 outline, const char* text)
	{
		if (!font)
			font = ImGui::GetFont();
		if (sz <= 0.f)
			sz = ImGui::GetFontSize();
		const float o = (sz >= 22.f) ? 2.f : 1.f;
		dl->AddText(font, sz, ImVec2(p.x + o, p.y + o), outline, text);
		dl->AddText(font, sz, ImVec2(p.x - o, p.y), outline, text);
		dl->AddText(font, sz, ImVec2(p.x + o, p.y), outline, text);
		dl->AddText(font, sz, ImVec2(p.x, p.y - o), outline, text);
		dl->AddText(font, sz, ImVec2(p.x, p.y + o), outline, text);
		dl->AddText(font, sz, p, fill, text);
	}

	const MumbleContext* Ctx()
	{
		if (!G::Mumble || G::Mumble->uiTick == 0)
			return nullptr;
		return reinterpret_cast<const MumbleContext*>(G::Mumble->context);
	}

	float WorldRadiusMeters(const MumbleContext* ctx)
	{
		float inches = 24.f;
		if (ctx)
		{
			switch (ctx->mountIndex)
			{
			case 1: case 4: case 7: case 8: /* Raptor / Griffon / Beetle / Skyscale */
				inches = 60.f;
				break;
			case 2: case 3: /* Springer / Jackal */
				inches = 50.f;
				break;
			case 5: /* Skimmer */
				inches = 66.f;
				break;
			case 6: /* Warclaw */
				inches = 40.f;
				break;
			case 9: /* Siege Turtle */
				inches = 80.f;
				break;
			default:
				inches = 24.f;
				break;
			}
		}
		inches += 24.f;
		const float scale = std::clamp(G::DirectionWorldRadiusScale, 0.4f, 3.0f);
		return inches * kInchesToMeters * scale;
	}

	Vec3 SmoothAvatar()
	{
		static std::deque<Vec3> hist;
		const Vec3 cur{
			G::Mumble->fAvatarPosition[0],
			G::Mumble->fAvatarPosition[1],
			G::Mumble->fAvatarPosition[2]};
		hist.push_back(cur);
		while (hist.size() > 12)
			hist.pop_front();
		Vec3 sum{};
		for (const Vec3& v : hist)
		{
			sum.x += v.x;
			sum.y += v.y;
			sum.z += v.z;
		}
		const float n = static_cast<float>(hist.size());
		return {sum.x / n, sum.y / n, sum.z / n};
	}

	void DrawWorldCardinals(float screenW, float screenH, float cameraYawDeg)
	{
		const MumbleContext* ctx = Ctx();
		Mat4 viewProj{};
		if (!BuildViewProj(screenW, screenH, viewProj))
			return;
		ImDrawList* dl = ImGui::GetBackgroundDrawList();
		if (!dl)
			return;

		const Vec3 origin = SmoothAvatar();
		if (!ReasonablePos(origin.x, origin.y, origin.z))
			return;

		const float* cp = G::Mumble->fCameraPosition;
		const float* cf = G::Mumble->fCameraFront;
		const float dx = cp[0] - origin.x;
		const float dy = cp[1] - origin.y;
		const float dz = cp[2] - origin.z;
		const float camDist = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (camDist < 2.5f && cf[1] > -0.5f)
			return;

		const float radius = WorldRadiusMeters(ctx);
		struct Card { const char* label; float angleDeg; };
		const Card cards[4] = {
			{"N", 0.f}, {"E", 90.f}, {"S", 180.f}, {"W", 270.f}};

		float camRot = Wrap360(cameraYawDeg);
		if (camRot <= 0.f)
			camRot = 360.f;

		for (const Card& card : cards)
		{
			const float rad = card.angleDeg * (kPi / 180.f);
			const Vec3 world{
				origin.x + radius * std::sin(rad),
				origin.y,
				origin.z + radius * std::cos(rad)};
			float sx = 0.f, sy = 0.f;
			if (!WorldToScreen(world, viewProj, screenW, screenH, sx, sy))
				continue;
			if (sx < -40.f || sy < -40.f || sx > screenW + 40.f || sy > screenH + 40.f)
				continue;

			const float fade = std::clamp(
				AngleDeltaAbs(card.angleDeg, camRot) / 45.f, 0.f, 1.f);
			const int a = static_cast<int>(fade * 255.f);
			if (a < 20)
				continue;

			ImFont* font = DirectionFontBig();
			const float fs = LetterPixelSize(font);
			const ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.f, card.label);
			const ImVec2 p{sx - ts.x * 0.5f, sy - ts.y * 0.5f};
			const ImU32 fill = (card.label[0] == 'N')
				? IM_COL32(240, 198, 90, a)
				: IM_COL32(240, 242, 245, a);
			DrawOutlinedText(dl, font, fs, p, fill, IM_COL32(0, 0, 0, a), card.label);
		}
	}
}

void DirectionCompass::DrawControls()
{
	PadNav::PushWrap();
	ImGui::TextColored(HelperTheme::Muted,
		"World N/E/S/W around your character. Reads Nexus FontBig; does not change Nexus fonts.");
	PadNav::PopWrap();
	if (ImGui::Checkbox("Enable direction compass###gw2igh_dircompass_pad", &G::ShowDirectionCompass))
		Settings::SetDirty();
	ImGui::SetNextItemWidth(-1.f);
	if (ImGui::SliderFloat("Letter size###gw2igh_dirletters_pad", &G::DirectionLetterScale, 0.5f, 2.5f, "%.2f×"))
		Settings::SetDirty();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(
			"Scales only our N/E/S/W draw size.\n"
			"1.00× = Nexus FontBig bake size. Does not touch FontGlobalScale.");
	ImGui::SetNextItemWidth(-1.f);
	if (ImGui::SliderFloat("World radius###gw2igh_dirradius_pad", &G::DirectionWorldRadiusScale, 0.4f, 3.0f, "%.2f×"))
		Settings::SetDirty();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("How far N/E/S/W sit from your character (hitbox base × this).");
}

void DirectionCompass::Open()
{
	G::ShowCompassPad = true;
	gRequestDock = true;
	Settings::SetDirty();
}

bool DirectionCompass::RenderPad()
{
	if (!G::ShowCompassPad)
		return false;

	constexpr float kPadW = 400.f;
	constexpr float kPadH = 280.f;

	ImGui::SetNextWindowSizeConstraints(ImVec2(300.f, 200.f),
		ImVec2(PadDock::MaxW(480.f), PadDock::MaxH(280.f)));
	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Appearing);
	PadDock::Place(G::PadCompass, gRequestDock, kPadW, kPadH, PadDock::BesideHelper(kPadW));
	if (!gRequestDock && G::PadCompass.w < 80.f)
		ImGui::SetNextWindowSize(ImVec2(kPadW, kPadH), ImGuiCond_FirstUseEver);

	bool open = G::ShowCompassPad;
	HelperTheme::ScopedWindow theme(G::Opacity);
	if (!ImGui::Begin("Compass###GW2InGameHelperCompass", &open))
	{
		if (PadDock::Capture(G::PadCompass))
			Settings::SetDirty();
		const bool hovered = ImGui::IsWindowHovered(
			ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
			ImGuiHoveredFlags_ChildWindows);
		ImGui::End();
		if (!open)
		{
			G::ShowCompassPad = false;
			Settings::SetDirty();
		}
		return hovered;
	}

	HelperTheme::ScopedFontScale fontScale(400.f, 280.f);
	DrawControls();

	if (PadDock::Capture(G::PadCompass))
		Settings::SetDirty();
	const bool hovered = ImGui::IsWindowHovered(
		ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
		ImGuiHoveredFlags_ChildWindows);
	ImGui::End();
	if (!open)
	{
		G::ShowCompassPad = false;
		Settings::SetDirty();
	}
	return hovered;
}

void DirectionCompass::Render()
{
	try
	{
		if (!G::ShowDirectionCompass)
			return;
		if (G::HideOutOfGameplay && G::NexusLink && !G::NexusLink->IsGameplay)
			return;
		if (!G::Mumble || G::Mumble->uiTick == 0)
			return;

		const MumbleContext* ctx = Ctx();
		if (!ctx || ctx->mapId == 0)
			return;
		if (G::HideWhenMapOpen &&
			(ctx->uiState & static_cast<uint32_t>(UiStateBits::MapOpen)) != 0)
			return;

		const ImGuiIO& io = ImGui::GetIO();
		const float screenW = io.DisplaySize.x;
		const float screenH = io.DisplaySize.y;
		if (screenW < 64.f || screenH < 64.f)
			return;

		DrawWorldCardinals(screenW, screenH, CameraYawDegrees());
	}
	catch (...)
	{
	}
}
