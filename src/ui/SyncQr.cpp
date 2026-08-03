#include "SyncQr.h"

#include "Sites.h"

#include "imgui/imgui.h"
#include "qrcodegen.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace SyncQr
{
	namespace
	{
		bool sShowPopup = false;
		char sUri[2048] = {};
		int sFavCount = 0;
		bool sEncodeOk = false;
		uint8_t sQr[qrcodegen_BUFFER_LEN_MAX] = {};
		int sQrSize = 0;
		char sStatus[128] = {};

		bool CopyTextToClipboard(const char* text)
		{
#ifdef _WIN32
			if (!text || !text[0])
				return false;
			const size_t len = std::strlen(text);
			if (!OpenClipboard(nullptr))
				return false;
			EmptyClipboard();
			HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
			if (!mem)
			{
				CloseClipboard();
				return false;
			}
			char* dst = static_cast<char*>(GlobalLock(mem));
			if (!dst)
			{
				GlobalFree(mem);
				CloseClipboard();
				return false;
			}
			std::memcpy(dst, text, len + 1);
			GlobalUnlock(mem);
			SetClipboardData(CF_TEXT, mem);
			CloseClipboard();
			return true;
#else
			(void)text;
			return false;
#endif
		}

		void RefreshPayload()
		{
			sEncodeOk = false;
			sQrSize = 0;
			sStatus[0] = 0;
			sFavCount = 0;
			sUri[0] = 0;
			if (!BuildFavoritesUri(sUri, sizeof(sUri), &sFavCount))
			{
				std::snprintf(sStatus, sizeof(sStatus), "No favorites to sync.");
				return;
			}
			uint8_t temp[qrcodegen_BUFFER_LEN_MAX];
			const bool ok = qrcodegen_encodeText(
				sUri,
				temp,
				sQr,
				qrcodegen_Ecc_MEDIUM,
				qrcodegen_VERSION_MIN,
				qrcodegen_VERSION_MAX,
				qrcodegen_Mask_AUTO,
				true);
			if (!ok)
			{
				std::snprintf(sStatus, sizeof(sStatus), "Favorites payload too large for QR.");
				return;
			}
			sQrSize = qrcodegen_getSize(sQr);
			sEncodeOk = sQrSize > 0;
		}

		void DrawQrCode(float maxSide)
		{
			if (!sEncodeOk || sQrSize <= 0)
				return;
			/* Quiet zone (light border) is required for phone cameras / ML Kit.
			   Without it on a dark ImGui window, scanners often never fire. */
			constexpr int kQuiet = 4;
			const int modules = sQrSize + kQuiet * 2;
			const ImVec2 origin = ImGui::GetCursorScreenPos();
			const float cell = maxSide / static_cast<float>(modules);
			const float side = cell * static_cast<float>(modules);
			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(origin, ImVec2(origin.x + side, origin.y + side), IM_COL32(255, 255, 255, 255));
			for (int y = 0; y < sQrSize; ++y)
			{
				for (int x = 0; x < sQrSize; ++x)
				{
					if (!qrcodegen_getModule(sQr, x, y))
						continue;
					const float x0 = origin.x + cell * static_cast<float>(x + kQuiet);
					const float y0 = origin.y + cell * static_cast<float>(y + kQuiet);
					dl->AddRectFilled(
						ImVec2(x0, y0),
						ImVec2(x0 + cell + 0.5f, y0 + cell + 0.5f),
						IM_COL32(0, 0, 0, 255));
				}
			}
			ImGui::Dummy(ImVec2(side, side));
		}
	}

	bool BuildFavoritesUri(char* out, size_t outLen, int* outCount)
	{
		if (!out || outLen < 64)
			return false;
		out[0] = 0;
		if (outCount)
			*outCount = 0;

		char csv[640] = {};
		Sites::SerializeFavorites(csv, sizeof(csv));
		if (!csv[0])
			return false;

		int n = 0;
		for (const char* p = csv; *p; ++p)
		{
			if (*p == ',')
				++n;
		}
		++n;
		if (outCount)
			*outCount = n;

		/* Versioned deep link — phone app + future seamless sync share this shape. */
		const int written = std::snprintf(
			out, outLen, "gw2helper://sync/v1?favorites=%s", csv);
		return written > 0 && static_cast<size_t>(written) < outLen;
	}

	void DrawOptionsSection()
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextUnformatted("Phone companion");
		ImGui::TextColored(
			ImVec4(0.65f, 0.65f, 0.68f, 1.f),
			"Sync favorites to the Android app with a QR code. Seamless sync can come later.");
		if (ImGui::Button("Show favorites QR…###gw2igh_qr"))
		{
			RefreshPayload();
			sShowPopup = true;
			ImGui::OpenPopup("GW2Helper Sync QR###gw2igh_qr_modal");
		}

		if (!sShowPopup)
			return;

		const ImGuiIO& io = ImGui::GetIO();
		const ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(420.f, 0.f), ImGuiCond_Appearing);
		bool open = sShowPopup;
		if (ImGui::BeginPopupModal("GW2Helper Sync QR###gw2igh_qr_modal", &open, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (sEncodeOk)
			{
				ImGui::Text("Favorites: %d", sFavCount);
				ImGui::TextWrapped("In the Android app: Settings → Scan favorites QR.");
				ImGui::Spacing();
				const float qrSide = 300.f;
				const float pad = (ImGui::GetContentRegionAvail().x - qrSide) * 0.5f;
				if (pad > 0.f)
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad);
				DrawQrCode(qrSide);
				ImGui::Spacing();
				if (ImGui::Button("Copy link###gw2igh_qr_copy", ImVec2(120.f, 0.f)))
				{
					if (CopyTextToClipboard(sUri))
						std::snprintf(sStatus, sizeof(sStatus), "Copied sync link.");
					else
						std::snprintf(sStatus, sizeof(sStatus), "Could not copy.");
				}
				ImGui::SameLine();
				if (ImGui::Button("Refresh###gw2igh_qr_refresh", ImVec2(120.f, 0.f)))
					RefreshPayload();
			}
			else
			{
				ImGui::TextWrapped("%s", sStatus[0] ? sStatus : "Nothing to show.");
			}
			if (sStatus[0] && sEncodeOk)
			{
				ImGui::Spacing();
				ImGui::TextColored(ImVec4(0.75f, 0.85f, 0.55f, 1.f), "%s", sStatus);
			}
			ImGui::Spacing();
			if (ImGui::Button("Close###gw2igh_qr_close", ImVec2(120.f, 0.f)))
			{
				open = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		sShowPopup = open;
	}
}
