#include "Gw2Ui.h"
#include "Gw2UiInternal.h"

#include "AddonPaths.h"
#include "Globals.h"
#include "UiChrome.h"

#include "imgui/imgui.h"
#include "nexus/Nexus.h"

#include <cstring>

namespace Gw2UiDetail
{
	Texture_t* GetChromeTex(int assetId)
	{
		if (assetId <= 0 || !G::API || !G::API->Textures_Get)
			return nullptr;
		char id[48];
		UiChrome::MakeTexId(assetId, id, sizeof(id));
		Texture_t* tex = G::API->Textures_Get(id);
		if (!tex || !tex->Resource)
		{
			/* Lazy warm if load raced ahead of extract. */
			UiChrome::WarmTextures(AddonPaths::DataDir());
			tex = G::API->Textures_Get(id);
		}
		if (tex && tex->Resource)
			return tex;
		/* CDN fallback (same IDs Tyrian Codex / Blish use) if pack missing. */
		Gw2Ui::Request(assetId);
		return Gw2UiDetail::GetTex(assetId);
	}

	Texture_t* GetChromeNamed(const char* stem)
	{
		if (!stem || !stem[0] || !G::API || !G::API->Textures_Get)
			return nullptr;
		char id[80];
		UiChrome::MakeNamedTexId(stem, id, sizeof(id));
		Texture_t* tex = G::API->Textures_Get(id);
		if (!tex || !tex->Resource)
		{
			UiChrome::WarmTextures(AddonPaths::DataDir());
			tex = G::API->Textures_Get(id);
		}
		if (!tex || !tex->Resource)
			return nullptr;
		return tex;
	}

	static void PaintEdgeStrip(ImDrawList* dl, ImTextureID iid, ImU32 col,
		ImVec2 p0, ImVec2 p1, bool omitLeft, bool omitRight, bool omitTop, bool omitBottom,
		float fringe, float bleed)
	{
		if (!dl || !iid)
			return;

		dl->PushClipRect(
			ImVec2(p0.x - bleed - 2.f, p0.y - (omitTop ? 0.f : bleed) - 2.f),
			ImVec2(p1.x + bleed + 2.f, p1.y + bleed + 2.f),
			false);

		if (!omitBottom)
		{
			dl->AddImage(iid,
				ImVec2(p0.x - 2.f, p1.y - fringe),
				ImVec2(p1.x + 2.f, p1.y + bleed),
				ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), col);
		}
		if (!omitTop)
		{
			dl->AddImage(iid,
				ImVec2(p0.x - 2.f, p0.y - bleed),
				ImVec2(p1.x + 2.f, p0.y + fringe),
				ImVec2(0.f, 1.f), ImVec2(1.f, 0.f), col);
		}
		if (!omitLeft)
		{
			dl->AddImageQuad(iid,
				ImVec2(p0.x - bleed, p0.y),
				ImVec2(p0.x + fringe, p0.y),
				ImVec2(p0.x + fringe, p1.y + bleed),
				ImVec2(p0.x - bleed, p1.y + bleed),
				ImVec2(0.f, 1.f), ImVec2(0.f, 0.f),
				ImVec2(1.f, 0.f), ImVec2(1.f, 1.f),
				col);
		}
		if (!omitRight)
		{
			dl->AddImageQuad(iid,
				ImVec2(p1.x - fringe, p0.y),
				ImVec2(p1.x + bleed, p0.y),
				ImVec2(p1.x + bleed, p1.y + bleed),
				ImVec2(p1.x - fringe, p1.y + bleed),
				ImVec2(0.f, 0.f), ImVec2(0.f, 1.f),
				ImVec2(1.f, 1.f), ImVec2(1.f, 0.f),
				col);
		}

		dl->PopClipRect();
	}

	void PaintHeroRim(ImDrawList* dl, ImVec2 p0, ImVec2 p1, float opacity,
		bool omitLeft, bool omitRight, bool omitTop, bool omitBottom)
	{
		if (!dl)
			return;
		float a = opacity;
		if (a < 0.f)
			a = 0.f;
		if (a > 1.f)
			a = 1.f;

		/* Tight metallic/ink rim (panel-edge) under a softer brush fringe (ink-edge). */
		Texture_t* edge = GetChromeNamed("panel-edge");
		if (edge && edge->Resource)
		{
			PaintEdgeStrip(dl, reinterpret_cast<ImTextureID>(edge->Resource),
				IM_COL32(255, 255, 255, static_cast<int>(a * 220.f + 0.5f)),
				p0, p1, omitLeft, omitRight, omitTop, omitBottom,
				/*fringe=*/11.f, /*bleed=*/4.f);
		}

		Texture_t* ink = GetChromeNamed("ink-edge");
		if (!ink || !ink->Resource)
			ink = GetChromeTex(static_cast<int>(Gw2Ui::Icon::InkEdge));
		if (ink && ink->Resource)
		{
			PaintEdgeStrip(dl, reinterpret_cast<ImTextureID>(ink->Resource),
				IM_COL32(255, 255, 255, static_cast<int>(a * 200.f + 0.5f)),
				p0, p1, omitLeft, omitRight, omitTop, omitBottom,
				/*fringe=*/22.f, /*bleed=*/14.f);
		}
	}
}

bool Gw2Ui::PaintPadChrome(float opacity, bool omitLeftEdge, bool omitRightEdge)
{
	/*
	 * Translucent wash plate (Hero lets the world show through) + dual rim:
	 * panel-edge (tight) then ink-edge (soft outer bleed). Top stays flush for title.
	 */
	ImDrawList* dl = ImGui::GetWindowDrawList();
	if (!dl)
		return false;

	float a = opacity;
	if (a < 0.f)
		a = 0.f;
	if (a > 1.f)
		a = 1.f;

	const ImVec2 p0 = ImGui::GetWindowPos();
	const ImVec2 sz = ImGui::GetWindowSize();
	const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);

	Texture_t* wash = Gw2UiDetail::GetChromeNamed("panel-wash");
	Texture_t* fill = (wash && wash->Resource) ? wash : Gw2UiDetail::GetChromeTex(static_cast<int>(Icon::PanelFill));
	if (!fill || !fill->Resource)
		fill = Gw2UiDetail::GetChromeTex(static_cast<int>(Icon::PanelFillAlt));
	const bool usingWash = (wash && wash->Resource && fill == wash);

	dl->PushClipRect(p0, p1, false);
	/* See-through underpaint — opaque plates kill the ink fringe silhouette. */
	dl->AddRectFilled(p0, p1, IM_COL32(10, 8, 6, static_cast<int>(a * 168.f + 0.5f)));

	if (!fill || !fill->Resource)
	{
		dl->AddRect(p0, p1, IM_COL32(161, 120, 56, static_cast<int>(a * 200.f + 0.5f)));
		dl->PopClipRect();
		return false;
	}

	const ImU32 washCol = IM_COL32(255, 255, 255, static_cast<int>(a * 205.f + 0.5f));
	ImVec2 uv0(0.f, 0.f), uv1(1.f, 1.f);
	if (!usingWash)
	{
		constexpr float tex = 1024.f;
		uv0 = ImVec2(48.f / tex, 34.f / tex);
		uv1 = ImVec2((40.f + 905.f) / tex, (26.f + 680.f) / tex);
	}
	dl->AddImage(reinterpret_cast<ImTextureID>(fill->Resource),
		p0, p1, uv0, uv1, washCol);
	dl->AddRectFilled(p0, p1, IM_COL32(6, 4, 3, static_cast<int>(a * 36.f + 0.5f)));
	dl->PopClipRect();

	/* Rim on this window's draw list so companion pads can still cover it. */
	Gw2UiDetail::PaintHeroRim(dl, p0, p1, a,
		omitLeftEdge, omitRightEdge, /*omitTop=*/true, /*omitBottom=*/false);

	return true;
}
