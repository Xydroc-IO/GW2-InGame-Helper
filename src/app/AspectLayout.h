#pragma once

/* Aspect-aware UI defaults for 16:9 / 21:9 / 32:9 (and nearby ratios).
   CEF OSR still caps at 1920×1200 — helper width is clamped to stay useful,
   not to fill a 5120px desktop. Pads use horizontal room beside the helper. */

namespace AspectLayout
{
	enum class Class : int
	{
		Standard_16_9 = 0,   /* includes 16:10 and common 16:9 */
		Ultrawide_21_9 = 1,  /* ~2.1–2.9 */
		Super_32_9 = 2       /* ~3.0+ (32:9 and similar) */
	};

	struct HelperGeom
	{
		float width = 1100.f;
		float height = 760.f;
		float posX = 60.f;
		float posY = 60.f;
		float maxW = 1680.f;
		float maxH = 1100.f;
	};

	struct BrowsePopupSize
	{
		float width = 540.f;
		float maxOuter = 400.f;
		float listMaxDefault = 260.f;
		float listMaxPicker = 300.f;
		float leftWMin = 140.f;
		float leftWMax = 180.f;
	};

	Class Classify(float displayW, float displayH);
	float AspectRatio(float displayW, float displayH);
	const char* ClassLabel(Class c);

	/* First-open helper size + preferred position (caller persists). */
	HelperGeom DefaultHelper(float displayW, float displayH);

	/* Browse category popup sizing. */
	BrowsePopupSize DefaultBrowsePopup(float displayW, float displayH);

	/* Opt-in auto font scale — height primary, mild aspect bump on ultrawide. */
	float SuggestFontScale(float displayW, float displayH);

	/* Remap a legacy “fraction of display width” pad X for ultrawide so pads
	   sit in the content band (near helper), not halfway across a 32:9 desk. */
	float PadFallbackX(float displayW, float displayH, float legacyFracX);
	float PadFallbackY(float displayH, float legacyFracY);
}
