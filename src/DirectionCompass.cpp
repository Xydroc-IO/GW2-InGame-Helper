#include "DirectionCompass.h"

#include "Globals.h"
#include "HelperTheme.h"
#include "Settings.h"

#include "imgui/imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>

/* Direction compass inspired by Raidcore GW2-Compass behavior (strip + world
   cardinals + indicator). Drawn from scratch with HelperTheme gold styling —
   no Raidcore source copied. */

namespace
{
	constexpr float kPi = 3.14159265358979323846f;
	constexpr float kDeg = 180.f / kPi;
	constexpr float kNearClip = 0.5f;
	constexpr float kFarClip = 8000.f;
	constexpr float kDefaultFov = 1.222f;
	constexpr float kInchesToMeters = 0.0254f;

	/* Strip (widget) — heading tape across the top, camera-relative. */
	constexpr float kStripWidth = 560.f;
	constexpr float kStripRangeDeg = 180.f;
	constexpr float kStripStepDeg = 15.f;
	constexpr float kStripHeight = 52.f;

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

	/* Camera yaw degrees — same atan2(X,Z) convention as Raidcore Compass. */
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

	ImU32 Col4(const ImVec4& c, float aMul = 1.f)
	{
		const int a = static_cast<int>(std::clamp(c.w * aMul, 0.f, 1.f) * 255.f);
		return IM_COL32(
			static_cast<int>(c.x * 255.f),
			static_cast<int>(c.y * 255.f),
			static_cast<int>(c.z * 255.f), a);
	}

	/* Same face Raidcore uses for world N/E/S/W (NexusLink FontBig). */
	ImFont* DirectionFontBig()
	{
		if (G::NexusLink && G::NexusLink->FontBig)
			return static_cast<ImFont*>(G::NexusLink->FontBig);
		return ImGui::GetFont();
	}

	float FontPixelSize(ImFont* font)
	{
		ImGui::PushFont(font);
		const float sz = ImGui::GetFontSize();
		ImGui::PopFont();
		return sz;
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

	const char* ShortBearing(int deg)
	{
		static const char* kNames[16] = {
			"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
			"S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
		int d = deg % 360;
		if (d < 0)
			d += 360;
		const int idx = static_cast<int>(std::floor((d + 11.25f) / 22.5f)) % 16;
		return kNames[idx];
	}

	const char* LongBearing(int deg)
	{
		static const char* kNames[16] = {
			"North", "North-northeast", "Northeast", "East-northeast",
			"East", "East-southeast", "Southeast", "South-southeast",
			"South", "South-southwest", "Southwest", "West-southwest",
			"West", "West-northwest", "Northwest", "North-northwest"};
		int d = deg % 360;
		if (d < 0)
			d += 360;
		const int idx = static_cast<int>(std::floor((d + 11.25f) / 22.5f)) % 16;
		return kNames[idx];
	}

	/* Notch label for strip — cardinals as letters, else degree. */
	std::string StripMarkerText(float markerDeg)
	{
		const int d = static_cast<int>(std::lround(Wrap360(markerDeg)));
		if (d == 0 || d == 360)
			return "N";
		if (d == 90)
			return "E";
		if (d == 180)
			return "S";
		if (d == 270)
			return "W";
		char buf[16];
		std::snprintf(buf, sizeof(buf), "%d", d);
		return buf;
	}

	const MumbleContext* Ctx()
	{
		if (!G::Mumble || G::Mumble->uiTick == 0)
			return nullptr;
		return reinterpret_cast<const MumbleContext*>(G::Mumble->context);
	}

	float WorldRadiusMeters(const MumbleContext* ctx)
	{
		/* Player hitbox ~24\" + padding 24\", mount scales — then inches→meters. */
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
		return inches * kInchesToMeters;
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

	void EnsureStripDefault(float screenW, float screenH)
	{
		if (!(G::DirectionWidgetX >= 0.f) || !(G::DirectionWidgetY >= 0.f))
		{
			G::DirectionWidgetX = (screenW - kStripWidth) * 0.5f;
			G::DirectionWidgetY = screenH * 0.085f;
		}
		if (!(G::DirectionIndicatorX >= 0.f) || !(G::DirectionIndicatorY >= 0.f))
		{
			G::DirectionIndicatorX = screenW * 0.5f - 60.f;
			G::DirectionIndicatorY = G::DirectionWidgetY + kStripHeight + 10.f;
		}
	}

	/* ---- Widget: scrolling heading strip (camera yaw) ---- */
	void DrawHeadingStrip(float cameraYawDeg)
	{
		const ImGuiIO& io = ImGui::GetIO();
		EnsureStripDefault(io.DisplaySize.x, io.DisplaySize.y);

		static bool sEditLive = false;
		if (!G::DirectionEditMode)
		{
			sEditLive = false;
			ImGui::SetNextWindowPos(ImVec2(G::DirectionWidgetX, G::DirectionWidgetY),
				ImGuiCond_Always);
		}
		else if (!sEditLive)
		{
			ImGui::SetNextWindowPos(ImVec2(G::DirectionWidgetX, G::DirectionWidgetY),
				ImGuiCond_Always);
			sEditLive = true;
		}
		ImGui::SetNextWindowSize(ImVec2(kStripWidth, kStripHeight), ImGuiCond_Always);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground;
		if (!G::DirectionEditMode)
			flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
		if (!ImGui::Begin("##gw2igh_dircompass_strip", nullptr, flags))
		{
			ImGui::End();
			ImGui::PopStyleVar(2);
			return;
		}

		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 wp = ImGui::GetWindowPos();
		const ImVec2 ws = ImGui::GetWindowSize();
		const float cx = wp.x + ws.x * 0.5f;
		const float top = wp.y + 4.f;
		const float midY = wp.y + ws.y * 0.55f;
		const float pxPerDeg = ws.x / kStripRangeDeg;

		/* Soft gold underlay for readability without a heavy panel. */
		dl->AddRectFilled(
			ImVec2(wp.x, wp.y), ImVec2(wp.x + ws.x, wp.y + ws.y),
			IM_COL32(6, 7, 10, 90), 4.f);
		dl->AddLine(ImVec2(wp.x + 8.f, midY), ImVec2(wp.x + ws.x - 8.f, midY),
			Col4(HelperTheme::Border, 0.55f), 1.0f);

		/* Center notch — current facing. */
		dl->AddTriangleFilled(
			ImVec2(cx, top + 2.f),
			ImVec2(cx - 7.f, top + 14.f),
			ImVec2(cx + 7.f, top + 14.f),
			Col4(HelperTheme::GoldBright));
		dl->AddLine(ImVec2(cx, top + 14.f), ImVec2(cx, midY + 10.f),
			Col4(HelperTheme::Gold), 1.6f);

		const float rot = cameraYawDeg; /* markers move opposite camera turn */
		const int steps = static_cast<int>(360.f / kStripStepDeg);
		for (int i = 0; i < steps; ++i)
		{
			const float markerDeg = kStripStepDeg * static_cast<float>(i);
			float rel = pxPerDeg * (markerDeg - rot);
			/* Wrap into visible tape (±1.5 widths). */
			const float wrap = ws.x * 2.f;
			while (rel > ws.x * 1.5f)
				rel -= wrap;
			while (rel < -ws.x * 0.5f)
				rel += wrap;

			const float mx = cx + rel;
			if (mx < wp.x + 2.f || mx > wp.x + ws.x - 2.f)
				continue;

			const std::string label = StripMarkerText(markerDeg);
			const bool cardinal = (label.size() == 1 &&
				(label[0] == 'N' || label[0] == 'E' || label[0] == 'S' || label[0] == 'W'));

			/* Edge fade toward tape ends and near center helper gap. */
			float t = 1.f;
			const float edge = 48.f;
			const float distLeft = mx - wp.x;
			const float distRight = (wp.x + ws.x) - mx;
			if (distLeft < edge)
				t *= distLeft / edge;
			if (distRight < edge)
				t *= distRight / edge;
			const float fromCenter = std::fabs(mx - cx);
			if (fromCenter < 18.f)
				t *= fromCenter / 18.f;
			t = std::clamp(t, 0.f, 1.f);
			if (t < 0.05f)
				continue;

			const float tickH = cardinal ? 14.f : 6.f;
			dl->AddLine(
				ImVec2(mx, midY - tickH), ImVec2(mx, midY + 2.f),
				cardinal ? Col4(HelperTheme::Gold, t) : Col4(HelperTheme::Muted, t * 0.85f),
				cardinal ? 1.8f : 1.0f);

			ImFont* font = cardinal ? DirectionFontBig() : ImGui::GetFont();
			const float fs = FontPixelSize(font);
			const ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.f, label.c_str());
			const ImVec2 tp{mx - ts.x * 0.5f, midY + 4.f};
			DrawOutlinedText(dl, font, fs, tp,
				cardinal ? Col4(HelperTheme::GoldBright, t) : Col4(HelperTheme::Muted, t),
				IM_COL32(0, 0, 0, static_cast<int>(200 * t)),
				label.c_str());
		}

		if (G::DirectionEditMode)
		{
			dl->AddRect(wp, ImVec2(wp.x + ws.x, wp.y + ws.y),
				Col4(HelperTheme::Gold, 0.7f), 3.f, 0, 1.f);
			const ImVec2 pos = ImGui::GetWindowPos();
			if (std::fabs(pos.x - G::DirectionWidgetX) > 0.5f ||
				std::fabs(pos.y - G::DirectionWidgetY) > 0.5f)
			{
				G::DirectionWidgetX = pos.x;
				G::DirectionWidgetY = pos.y;
				Settings::SetDirty();
			}
		}

		ImGui::End();
		ImGui::PopStyleVar(2);
	}

	/* ---- World: N/E/S/W around the character at hitbox radius ---- */
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
		/* Hide when camera is practically on top of the agent (same idea as Raidcore). */
		if (camDist < 2.5f && cf[1] > -0.5f)
			return;

		const float radius = WorldRadiusMeters(ctx);
		/* +Z north, +X east — matches common GW2 Mumble yaw (atan2 X,Z). */
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

			/* Fade letters that sit behind the camera view direction. */
			const float fade = std::clamp(
				AngleDeltaAbs(card.angleDeg, camRot) / 45.f, 0.f, 1.f);
			const int a = static_cast<int>(fade * 255.f);
			if (a < 20)
				continue;

			ImFont* font = DirectionFontBig();
			const float fs = FontPixelSize(font);
			const ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.f, card.label);
			const ImVec2 p{sx - ts.x * 0.5f, sy - ts.y * 0.5f};
			const ImU32 fill = (card.label[0] == 'N')
				? IM_COL32(240, 198, 90, a)
				: IM_COL32(240, 242, 245, a);
			DrawOutlinedText(dl, font, fs, p, fill, IM_COL32(0, 0, 0, a), card.label);
		}
	}

	/* ---- Indicator: readable bearing under the strip ---- */
	void DrawIndicator(float cameraYawDeg)
	{
		const int iRot = static_cast<int>(std::lround(cameraYawDeg));
		char line[96];
		std::snprintf(line, sizeof(line), "%d°  %s", iRot, LongBearing(iRot));

		const ImVec2 textSize = ImGui::CalcTextSize(line);
		const ImVec2 pad{10.f, 5.f};
		static bool sEditLive = false;
		if (!G::DirectionEditMode)
		{
			sEditLive = false;
			ImGui::SetNextWindowPos(ImVec2(G::DirectionIndicatorX, G::DirectionIndicatorY),
				ImGuiCond_Always);
		}
		else if (!sEditLive)
		{
			ImGui::SetNextWindowPos(ImVec2(G::DirectionIndicatorX, G::DirectionIndicatorY),
				ImGuiCond_Always);
			sEditLive = true;
		}
		ImGui::SetNextWindowSize(
			ImVec2(textSize.x + pad.x * 2.f, textSize.y + pad.y * 2.f), ImGuiCond_Always);

		ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground;
		if (!G::DirectionEditMode)
			flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, pad);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
		if (ImGui::Begin("##gw2igh_dircompass_ind", nullptr, flags))
		{
			ImDrawList* dl = ImGui::GetWindowDrawList();
			const ImVec2 p = ImGui::GetCursorScreenPos();
			DrawOutlinedText(dl, ImGui::GetFont(), ImGui::GetFontSize(), p,
				Col4(HelperTheme::GoldBright), IM_COL32(0, 0, 0, 210), line);
			ImGui::Dummy(textSize);
			if (ImGui::IsWindowHovered())
				ImGui::SetTooltip("%s", ShortBearing(iRot));

			if (G::DirectionEditMode)
			{
				const ImVec2 wp = ImGui::GetWindowPos();
				const ImVec2 ws = ImGui::GetWindowSize();
				dl->AddRect(wp, ImVec2(wp.x + ws.x, wp.y + ws.y),
					Col4(HelperTheme::Gold, 0.7f), 3.f, 0, 1.f);
				const ImVec2 pos = ImGui::GetWindowPos();
				if (std::fabs(pos.x - G::DirectionIndicatorX) > 0.5f ||
					std::fabs(pos.y - G::DirectionIndicatorY) > 0.5f)
				{
					G::DirectionIndicatorX = pos.x;
					G::DirectionIndicatorY = pos.y;
					Settings::SetDirty();
				}
			}
		}
		ImGui::End();
		ImGui::PopStyleVar(2);
	}
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

		EnsureStripDefault(screenW, screenH);
		const float camYaw = CameraYawDegrees();

		if (G::ShowDirectionWorld)
			DrawWorldCardinals(screenW, screenH, camYaw);
		if (G::ShowDirectionWidget)
			DrawHeadingStrip(camYaw);
		if (G::ShowDirectionIndicator)
			DrawIndicator(camYaw);

		if (G::DirectionEditMode)
		{
			ImDrawList* fg = ImGui::GetForegroundDrawList();
			if (fg)
			{
				DrawOutlinedText(fg, ImGui::GetFont(), ImGui::GetFontSize(),
					ImVec2(12.f, screenH - 28.f),
					Col4(HelperTheme::GoldBright), IM_COL32(0, 0, 0, 220),
					"Compass edit: drag strip / indicator — turn off Edit in More when done");
			}
		}
	}
	catch (...)
	{
	}
}
