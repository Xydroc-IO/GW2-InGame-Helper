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
}

bool Gw2Ui::PaintPadChrome(float opacity)
{
	/*
	 * Full-bleed wash plate (no black matte inset). Soft ink fringe only on
	 * L/R/bottom so the rim feathers into the game like Hero — not a thick frame.
	 * Top stays flush for the title bar.
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
	dl->AddRectFilled(p0, p1, IM_COL32(10, 8, 6, static_cast<int>(a * 255.f + 0.5f)));

	if (!fill || !fill->Resource)
	{
		dl->AddRect(p0, p1, IM_COL32(161, 120, 56, static_cast<int>(a * 200.f + 0.5f)));
		dl->PopClipRect();
		return false;
	}

	const ImU32 col = IM_COL32(255, 255, 255, static_cast<int>(a * 255.f + 0.5f));
	ImVec2 uv0(0.f, 0.f), uv1(1.f, 1.f);
	if (!usingWash)
	{
		constexpr float tex = 1024.f;
		uv0 = ImVec2(48.f / tex, 34.f / tex);
		uv1 = ImVec2((40.f + 905.f) / tex, (26.f + 680.f) / tex);
	}
	dl->AddImage(reinterpret_cast<ImTextureID>(fill->Resource),
		p0, p1, uv0, uv1, col);
	dl->AddRectFilled(p0, p1, IM_COL32(6, 4, 3, static_cast<int>(a * 55.f + 0.5f)));
	dl->PopClipRect();

	/* Soft brush fringe only — never a solid black mat around the window. */
	Texture_t* ink = Gw2UiDetail::GetChromeNamed("ink-edge");
	if (!ink || !ink->Resource)
		ink = Gw2UiDetail::GetChromeTex(static_cast<int>(Icon::InkEdge));
	if (ink && ink->Resource)
	{
		constexpr float kFringe = 18.f;
		constexpr float kBleed = 10.f;
		ImDrawList* edgeDl = ImGui::GetForegroundDrawList();
		const ImTextureID iid = reinterpret_cast<ImTextureID>(ink->Resource);
		const ImU32 inkCol = IM_COL32(255, 255, 255, static_cast<int>(a * 180.f + 0.5f));

		edgeDl->PushClipRect(
			ImVec2(p0.x - kBleed - 2.f, p0.y),
			ImVec2(p1.x + kBleed + 2.f, p1.y + kBleed + 2.f),
			false);

		edgeDl->AddImage(iid,
			ImVec2(p0.x - 2.f, p1.y - kFringe),
			ImVec2(p1.x + 2.f, p1.y + kBleed),
			ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), inkCol);

		edgeDl->AddImageQuad(iid,
			ImVec2(p0.x - kBleed, p0.y),
			ImVec2(p0.x + kFringe, p0.y),
			ImVec2(p0.x + kFringe, p1.y + kBleed),
			ImVec2(p0.x - kBleed, p1.y + kBleed),
			ImVec2(0.f, 1.f), ImVec2(0.f, 0.f),
			ImVec2(1.f, 0.f), ImVec2(1.f, 1.f),
			inkCol);

		edgeDl->AddImageQuad(iid,
			ImVec2(p1.x - kFringe, p0.y),
			ImVec2(p1.x + kBleed, p0.y),
			ImVec2(p1.x + kBleed, p1.y + kBleed),
			ImVec2(p1.x - kFringe, p1.y + kBleed),
			ImVec2(0.f, 0.f), ImVec2(0.f, 1.f),
			ImVec2(1.f, 1.f), ImVec2(1.f, 0.f),
			inkCol);

		edgeDl->PopClipRect();
	}

	return true;
}
