#include "AspectLayout.h"

#include <algorithm>
#include <cmath>

namespace AspectLayout
{
	namespace
	{
		float Clampf(float v, float lo, float hi)
		{
			if (v < lo) return lo;
			if (v > hi) return hi;
			return v;
		}
	}

	float AspectRatio(float displayW, float displayH)
	{
		if (!(displayW > 100.f) || !(displayH > 100.f))
			return 16.f / 9.f;
		return displayW / displayH;
	}

	Class Classify(float displayW, float displayH)
	{
		const float a = AspectRatio(displayW, displayH);
		/* 32:9 ≈ 3.56; 21:9 ≈ 2.33; 16:9 ≈ 1.78. Hysteresis-ish bands. */
		if (a >= 2.95f)
			return Class::Super_32_9;
		if (a >= 2.05f)
			return Class::Ultrawide_21_9;
		return Class::Standard_16_9;
	}

	const char* ClassLabel(Class c)
	{
		switch (c)
		{
		case Class::Ultrawide_21_9: return "21:9";
		case Class::Super_32_9: return "32:9";
		case Class::Standard_16_9:
		default: return "16:9";
		}
	}

	HelperGeom DefaultHelper(float displayW, float displayH)
	{
		HelperGeom g{};
		if (!(displayW > 100.f) || !(displayH > 100.f))
			return g;

		const Class c = Classify(displayW, displayH);
		switch (c)
		{
		case Class::Ultrawide_21_9:
			/* Wider than 16:9 defaults but stay near CEF-useful widths. */
			g.width = Clampf(displayW * 0.36f, 880.f, 1680.f);
			g.height = Clampf(displayH * 0.58f, 520.f, 1080.f);
			g.maxW = Clampf(displayW * 0.72f, 1400.f, 1920.f);
			g.maxH = Clampf(displayH * 0.92f, 900.f, 1200.f);
			g.posX = Clampf(displayW * 0.06f, 24.f, displayW * 0.20f);
			g.posY = Clampf(displayH * 0.08f, 24.f, displayH * 0.18f);
			break;
		case Class::Super_32_9:
			/* Cap near OSR max width — do not stretch CEF across 5120px. */
			g.width = Clampf(std::min(displayW * 0.28f, 1760.f), 960.f, 1920.f);
			g.height = Clampf(displayH * 0.62f, 540.f, 1100.f);
			g.maxW = 1920.f;
			g.maxH = Clampf(displayH * 0.92f, 900.f, 1200.f);
			/* Sit in the left content third so pads can open to the right. */
			g.posX = Clampf((displayW - g.width) * 0.18f, 32.f, displayW * 0.28f);
			g.posY = Clampf(displayH * 0.08f, 24.f, displayH * 0.16f);
			break;
		case Class::Standard_16_9:
		default:
			g.width = Clampf(displayW * 0.42f, 780.f, 1480.f);
			g.height = Clampf(displayH * 0.56f, 500.f, 1040.f);
			g.maxW = Clampf(displayW * 0.92f, 1200.f, 1920.f);
			g.maxH = Clampf(displayH * 0.92f, 800.f, 1200.f);
			g.posX = Clampf(displayW * 0.05f, 24.f, 120.f);
			g.posY = Clampf(displayH * 0.07f, 24.f, 100.f);
			break;
		}
		return g;
	}

	BrowsePopupSize DefaultBrowsePopup(float displayW, float displayH)
	{
		BrowsePopupSize s{};
		if (!(displayW > 100.f) || !(displayH > 100.f))
			return s;

		switch (Classify(displayW, displayH))
		{
		case Class::Ultrawide_21_9:
			s.width = Clampf(displayW * 0.22f, 520.f, 760.f);
			s.maxOuter = Clampf(displayH * 0.40f, 320.f, 560.f);
			s.listMaxDefault = Clampf(displayH * 0.22f, 180.f, 300.f);
			s.listMaxPicker = Clampf(displayH * 0.26f, 200.f, 340.f);
			s.leftWMin = 150.f;
			s.leftWMax = 200.f;
			break;
		case Class::Super_32_9:
			s.width = Clampf(std::min(displayW * 0.16f, 720.f), 540.f, 720.f);
			s.maxOuter = Clampf(displayH * 0.42f, 340.f, 580.f);
			s.listMaxDefault = Clampf(displayH * 0.24f, 190.f, 320.f);
			s.listMaxPicker = Clampf(displayH * 0.28f, 210.f, 360.f);
			s.leftWMin = 150.f;
			s.leftWMax = 200.f;
			break;
		case Class::Standard_16_9:
		default:
			s.width = Clampf(displayW * 0.30f, 500.f, 700.f);
			s.maxOuter = Clampf(displayH * 0.38f, 320.f, 520.f);
			s.listMaxDefault = Clampf(displayH * 0.22f, 170.f, 280.f);
			s.listMaxPicker = Clampf(displayH * 0.26f, 190.f, 320.f);
			s.leftWMin = 140.f;
			s.leftWMax = 190.f;
			break;
		}
		return s;
	}

	float SuggestFontScale(float displayW, float displayH)
	{
		float s = 1.f;
		if (displayH > 1600.f)
			s = Clampf(displayH / 1440.f, 1.f, 1.12f);
		else if (displayH > 1400.f)
			s = Clampf(displayH / 1440.f, 1.f, 1.08f);

		/* Ultrawide desks often run farther from the screen — tiny bump only. */
		switch (Classify(displayW, displayH))
		{
		case Class::Ultrawide_21_9:
			s = Clampf(s * 1.03f, 0.85f, 1.15f);
			break;
		case Class::Super_32_9:
			s = Clampf(s * 1.04f, 0.85f, 1.15f);
			break;
		default:
			s = Clampf(s, 0.85f, 1.15f);
			break;
		}
		return s;
	}

	float PadFallbackX(float displayW, float displayH, float legacyFracX)
	{
		if (!(displayW > 100.f))
			return 120.f;
		const float frac = Clampf(legacyFracX, 0.05f, 0.90f);
		switch (Classify(displayW, displayH))
		{
		case Class::Ultrawide_21_9:
			/* Compress toward left-center content band. */
			return displayW * Clampf(0.12f + frac * 0.55f, 0.10f, 0.72f);
		case Class::Super_32_9:
			return displayW * Clampf(0.10f + frac * 0.42f, 0.08f, 0.55f);
		default:
			return displayW * frac;
		}
	}

	float PadFallbackY(float displayH, float legacyFracY)
	{
		if (!(displayH > 100.f))
			return 80.f;
		return displayH * Clampf(legacyFracY, 0.02f, 0.85f);
	}
}
