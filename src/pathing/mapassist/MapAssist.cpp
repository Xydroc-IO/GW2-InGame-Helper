#include "MapAssist.h"

#include "GameLive.h"
#include "Globals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <windows.h>

namespace
{
	/* Tuning is local to this addon — measured against Mumble MapCenter feedback. */
	constexpr int kStrokeSamples = 4;           /* points along one drag stroke */
	constexpr int kSteerMinRounds = 8;
	constexpr int kSteerMaxRounds = 36;
	constexpr double kMapSampleSec = 0.28;      /* wait for MapCenter after a stroke */
	constexpr double kTapSettleSec = 0.25;
	constexpr double kMapOpenRetrySec = 0.45;
	constexpr float kArriveContinent = 240.f;   /* stop when MapCenter is this close */
	constexpr float kGainStart = 0.40f;
	constexpr float kGainMin = 0.12f;
	constexpr float kGainMax = 2.0f;

	enum class Stage : int
	{
		Idle = 0,
		AwaitingMap,
		Steering,
		TapWaypoint,
		Finished
	};

	Stage gStage = Stage::Idle;
	float gGoalX = 0.f;
	float gGoalY = 0.f;
	bool gTapAfterSteer = false;
	int gSteerRound = 0;
	int gSteerCap = kSteerMinRounds;
	double gWakeAt = 0.0;
	float gGain = kGainStart;
	char gStatus[96] = {};
	POINT gSavedCursor{};
	bool gSavedCursorOk = false;
	float gSampleCx = 0.f;
	float gSampleCy = 0.f;
	bool gHaveSample = false;

	double ClockSec()
	{
		return static_cast<double>(GetTickCount64()) / 1000.0;
	}

	void SetMsg(const char* s)
	{
		std::snprintf(gStatus, sizeof(gStatus), "%s", s ? s : "");
	}

	LONG AbsAxis(int pixel, int origin, int extent)
	{
		if (extent <= 1)
			return 0;
		return static_cast<LONG>(std::llround(
			(static_cast<double>(pixel - origin) * 65535.0) /
			static_cast<double>(extent - 1)));
	}

	bool EmitAbsMove(POINT screen)
	{
		const int ox = GetSystemMetrics(SM_XVIRTUALSCREEN);
		const int oy = GetSystemMetrics(SM_YVIRTUALSCREEN);
		const int ow = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		const int oh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
		INPUT in{};
		in.type = INPUT_MOUSE;
		in.mi.dx = AbsAxis(screen.x, ox, ow);
		in.mi.dy = AbsAxis(screen.y, oy, oh);
		in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
		return SendInput(1, &in, sizeof(INPUT)) == 1;
	}

	bool EmitButton(DWORD flags)
	{
		INPUT in{};
		in.type = INPUT_MOUSE;
		in.mi.dwFlags = flags;
		return SendInput(1, &in, sizeof(INPUT)) == 1;
	}

	/* Smooth ease-out stroke as separate events (not one multi-INPUT burst). */
	bool StrokeDrag(POINT from, POINT to)
	{
		if (!EmitAbsMove(from) || !EmitButton(MOUSEEVENTF_LEFTDOWN))
			return false;
		for (int i = 1; i <= kStrokeSamples; ++i)
		{
			const float t = static_cast<float>(i) / static_cast<float>(kStrokeSamples);
			const float e = 1.f - (1.f - t) * (1.f - t); /* ease-out quad */
			POINT p;
			p.x = from.x + static_cast<LONG>((to.x - from.x) * e);
			p.y = from.y + static_cast<LONG>((to.y - from.y) * e);
			if (!EmitAbsMove(p))
			{
				EmitButton(MOUSEEVENTF_LEFTUP);
				return false;
			}
		}
		return EmitButton(MOUSEEVENTF_LEFTUP);
	}

	bool StrokeTap(POINT at)
	{
		return EmitAbsMove(at) &&
			EmitButton(MOUSEEVENTF_LEFTDOWN) &&
			EmitButton(MOUSEEVENTF_LEFTUP);
	}

	bool ReadMapView(float& cx, float& cy, float& scale)
	{
		if (!G::Mumble || G::Mumble->context_len < sizeof(MumbleContext))
			return false;
		const auto* ctx = reinterpret_cast<const MumbleContext*>(G::Mumble->context);
		cx = ctx->mapCenterX;
		cy = ctx->mapCenterY;
		scale = ctx->mapScale;
		return scale > 0.01f;
	}

	void RequestMapToggle()
	{
		if (G::API && G::API->GameBinds_InvokeAsync)
			G::API->GameBinds_InvokeAsync(GB_MapToggle, 0);
	}

	void ClientSize(float& w, float& h, float& uiScale)
	{
		w = 1920.f;
		h = 1080.f;
		uiScale = 1.f;
		if (G::NexusLink)
		{
			if (G::NexusLink->Width > 0)
				w = static_cast<float>(G::NexusLink->Width);
			if (G::NexusLink->Height > 0)
				h = static_cast<float>(G::NexusLink->Height);
			if (G::NexusLink->Scaling > 0.1f)
				uiScale = G::NexusLink->Scaling;
		}
	}

	void EnterSteering()
	{
		gStage = Stage::Steering;
		gSteerRound = 0;
		gSteerCap = kSteerMinRounds;
		gGain = kGainStart;
		gHaveSample = false;
		gWakeAt = ClockSec();
		SetMsg("Steering map…");
	}

	void RestoreCursor()
	{
		if (gSavedCursorOk)
		{
			SetCursorPos(gSavedCursor.x, gSavedCursor.y);
			gSavedCursorOk = false;
		}
	}

	void RememberCursor()
	{
		if (!gSavedCursorOk)
		{
			GetCursorPos(&gSavedCursor);
			gSavedCursorOk = true;
		}
	}
}

bool MapAssist::Enabled()
{
	return G::MapAssistEnabled;
}

bool MapAssist::ClickWaypointEnabled()
{
	return G::MapAssistClickWaypoint;
}

void MapAssist::Cancel()
{
	gStage = Stage::Idle;
	gTapAfterSteer = false;
	gHaveSample = false;
	RestoreCursor();
	SetMsg("");
}

bool MapAssist::Busy()
{
	return gStage != Stage::Idle && gStage != Stage::Finished;
}

const char* MapAssist::Status()
{
	return gStatus;
}

void MapAssist::RequestPanTo(float continentX, float continentY)
{
	if (!Enabled())
	{
		SetMsg("Map assist off (Pathing → Overview).");
		return;
	}
	gGoalX = continentX;
	gGoalY = continentY;
	gTapAfterSteer = false;
	if (!GameLive::IsMapOpen())
	{
		gStage = Stage::AwaitingMap;
		RequestMapToggle();
		gWakeAt = ClockSec() + kMapOpenRetrySec;
		SetMsg("Opening world map…");
		return;
	}
	EnterSteering();
}

void MapAssist::RequestTravelAssist(float continentX, float continentY)
{
	RequestPanTo(continentX, continentY);
	gTapAfterSteer = ClickWaypointEnabled();
}

void MapAssist::OpenMapAndPanTo(float continentX, float continentY)
{
	RequestPanTo(continentX, continentY);
}

void MapAssist::Tick()
{
	if (gStage == Stage::Idle || gStage == Stage::Finished)
		return;
	if (!Enabled())
	{
		Cancel();
		return;
	}
	if (!GameLive::IsLive() || !GameLive::GameHasFocus() || GameLive::TextboxHasFocus())
		return;
	/* Do not wrestle the player for the mouse. */
	if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) || (GetAsyncKeyState(VK_RBUTTON) & 0x8000))
		return;

	const double now = ClockSec();
	if (now < gWakeAt)
		return;

	if (gStage == Stage::AwaitingMap)
	{
		if (!GameLive::IsMapOpen())
		{
			RequestMapToggle();
			gWakeAt = now + kMapOpenRetrySec;
			return;
		}
		EnterSteering();
		return;
	}

	if (gStage == Stage::Steering)
	{
		if (!GameLive::IsMapOpen())
		{
			SetMsg("Map closed — assist stopped.");
			gStage = Stage::Idle;
			RestoreCursor();
			return;
		}

		float cx = 0.f, cy = 0.f, scale = 1.f;
		if (!ReadMapView(cx, cy, scale))
		{
			gWakeAt = now + kMapSampleSec;
			return;
		}

		/* Calibrate gain from last stroke's observed MapCenter delta. */
		if (gHaveSample && gSteerRound > 0)
		{
			const float moved = std::hypot(cx - gSampleCx, cy - gSampleCy);
			if (moved > 25.f)
			{
				/* Prefer slightly stronger strokes when under-shooting. */
				const float errWas = std::hypot(gGoalX - gSampleCx, gGoalY - gSampleCy);
				if (errWas > 40.f && moved < errWas * 0.35f)
					gGain = std::clamp(gGain * 1.15f, kGainMin, kGainMax);
				else if (moved > errWas * 1.25f)
					gGain = std::clamp(gGain * 0.85f, kGainMin, kGainMax);
			}
		}

		const float dx = gGoalX - cx;
		const float dy = gGoalY - cy;
		const float err = std::hypot(dx, dy);
		if (err < kArriveContinent)
		{
			if (gTapAfterSteer && ClickWaypointEnabled())
			{
				gStage = Stage::TapWaypoint;
				gWakeAt = now + kTapSettleSec;
				SetMsg("Tapping waypoint…");
			}
			else
			{
				SetMsg("Map centered — you can travel manually.");
				gStage = Stage::Finished;
				RestoreCursor();
			}
			return;
		}

		if (gSteerRound >= gSteerCap)
		{
			if (gSteerCap < kSteerMaxRounds)
				gSteerCap = std::min(kSteerMaxRounds, gSteerCap + 8);
			else
			{
				SetMsg("Could not finish pan — try zooming the map.");
				gStage = Stage::Finished;
				RestoreCursor();
				return;
			}
		}

		float screenW = 0.f, screenH = 0.f, uiScale = 1.f;
		ClientSize(screenW, screenH, uiScale);
		const float mapScale = scale / uiScale;
		/* Drag opposite continent error so MapCenter moves toward the goal. */
		float strokeX = (-dx / mapScale) * gGain;
		float strokeY = (-dy / mapScale) * gGain;
		const float strokeCap = std::clamp(
			std::min(screenW, screenH) * 0.35f, 180.f, 480.f);
		const float strokeLen = std::hypot(strokeX, strokeY);
		if (strokeLen > strokeCap && strokeLen > 1.f)
		{
			strokeX *= strokeCap / strokeLen;
			strokeY *= strokeCap / strokeLen;
		}

		RememberCursor();
		const int midX = GetSystemMetrics(SM_XVIRTUALSCREEN) +
			GetSystemMetrics(SM_CXVIRTUALSCREEN) / 2;
		const int midY = GetSystemMetrics(SM_YVIRTUALSCREEN) +
			GetSystemMetrics(SM_CYVIRTUALSCREEN) / 2;
		POINT from{ midX, midY };
		POINT to{
			midX + static_cast<LONG>(strokeX),
			midY + static_cast<LONG>(strokeY)
		};

		gSampleCx = cx;
		gSampleCy = cy;
		gHaveSample = true;
		if (!StrokeDrag(from, to))
		{
			SetMsg("Cursor input failed.");
			gStage = Stage::Finished;
			RestoreCursor();
			return;
		}
		++gSteerRound;
		gWakeAt = now + kMapSampleSec;
		SetCursorPos(gSavedCursor.x, gSavedCursor.y);
		char buf[80];
		std::snprintf(buf, sizeof(buf), "Steering… %d/%d (%.0f cont)",
			gSteerRound, gSteerCap, err);
		SetMsg(buf);
		return;
	}

	if (gStage == Stage::TapWaypoint)
	{
		if (!gTapAfterSteer || !ClickWaypointEnabled())
		{
			SetMsg("Ready — confirm any teleport prompt yourself.");
			gStage = Stage::Finished;
			RestoreCursor();
			return;
		}
		float cx = 0.f, cy = 0.f, scale = 1.f;
		if (!ReadMapView(cx, cy, scale))
		{
			gWakeAt = now + kTapSettleSec;
			return;
		}
		float screenW = 0.f, screenH = 0.f, uiScale = 1.f;
		ClientSize(screenW, screenH, uiScale);
		const float mapScale = scale / uiScale;
		const float sx = screenW * 0.5f + (gGoalX - cx) / mapScale;
		const float sy = screenH * 0.5f + (gGoalY - cy) / mapScale;
		POINT p{ static_cast<LONG>(sx), static_cast<LONG>(sy) };
		if (HWND hwnd = GetForegroundWindow())
			ClientToScreen(hwnd, &p);
		StrokeTap(p);
		gTapAfterSteer = false;
		SetMsg("Teleport prompt opened — confirm in-game.");
		gStage = Stage::Finished;
		RestoreCursor();
	}
}
