/* CEF OSR paint + popup compositing — HelperDetail. */
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <vector>

#include "HelperInternal.h"
#include "WikiIpc.h"

#include "include/capi/cef_browser_capi.h"
#include "include/capi/cef_render_handler_capi.h"
#include "include/internal/cef_types.h"

namespace HelperDetail
{
	void CEF_CALLBACK GetViewRect(cef_render_handler_t*, cef_browser_t*, cef_rect_t* rect)
	{
		int w = 800, h = 600;
		ViewSize(&w, &h);
		rect->x = 0;
		rect->y = 0;
		rect->width = w;
		rect->height = h;
	}

	/* Desktop monitor + work area for OSR screen metrics. Keep separate from
	   GetViewRect (ImGui panel size) so JS screen.width/height look like a real
	   display — matching view size is a common anti-bot / non-billable signal.
	   device_scale_factor stays 1.0: IPC mouse + OnPaint are already view pixels. */
	void FillDesktopScreenRects(cef_rect_t* monitor, cef_rect_t* work)
	{
		int screenW = GetSystemMetrics(SM_CXSCREEN);
		int screenH = GetSystemMetrics(SM_CYSCREEN);
		if (screenW < 800) screenW = 800;
		if (screenH < 600) screenH = 600;

		monitor->x = 0;
		monitor->y = 0;
		monitor->width = screenW;
		monitor->height = screenH;
		*work = *monitor;

		RECT wa{};
		if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0))
		{
			const int ww = wa.right - wa.left;
			const int wh = wa.bottom - wa.top;
			if (ww >= 640 && wh >= 480)
			{
				work->x = wa.left;
				work->y = wa.top;
				work->width = ww;
				work->height = wh;
			}
		}
	}

	int CEF_CALLBACK GetScreenInfo(cef_render_handler_t*, cef_browser_t*, cef_screen_info_t* info)
	{
		if (!info)
			return 0;
		info->size = sizeof(*info);
		/* Do not use ViewSize here — that is the browser viewport only. */
		info->device_scale_factor = 1.0f;
		info->depth = 32;
		info->depth_per_component = 8;
		info->is_monochrome = 0;
		FillDesktopScreenRects(&info->rect, &info->available_rect);
		return 1;
	}

	bool IsOverPopup(int x, int y)
	{
		return gPopupShow &&
			x >= gPopupRect.x && y >= gPopupRect.y &&
			x < gPopupRect.x + gPopupRect.width &&
			y < gPopupRect.y + gPopupRect.height;
	}

	void ApplyPopupMouseOffset(int& x, int& y)
	{
		if (!IsOverPopup(x, y))
			return;
		x -= gPopupRect.x;
		y -= gPopupRect.y;
	}

	void PublishCompositedFrame(int dirtyX, int dirtyY, int dirtyW, int dirtyH)
	{
		if (!gFramePixels || !gIpc || gViewCacheW <= 0 || gViewCacheH <= 0)
			return;
		if (gViewCacheW > static_cast<int>(kWikiFrameMaxW) ||
			gViewCacheH > static_cast<int>(kWikiFrameMaxH))
			return;

		const uint32_t front = gIpc->frame_front & 1u;
		const uint32_t back = 1u - front;
		if (gIpc->frame_reading == back)
			return;

		uint8_t* dstBase = gFramePixels + static_cast<size_t>(back) * kWikiFrameBytes;
		const size_t rowBytes = static_cast<size_t>(gViewCacheW) * 4;
		for (int y = 0; y < gViewCacheH; ++y)
		{
			std::memcpy(
				dstBase + static_cast<size_t>(y) * kWikiFrameStride,
				gViewCache.data() + static_cast<size_t>(y) * rowBytes,
				rowBytes);
		}

		int ux = dirtyX, uy = dirtyY, uw = dirtyW, uh = dirtyH;
		if (gPopupShow && gPopupCacheW > 0 && gPopupCacheH > 0 &&
			!gPopupCache.empty() && gPopupRect.width > 0 && gPopupRect.height > 0)
		{
			const int pw = gPopupCacheW < gPopupRect.width ? gPopupCacheW : gPopupRect.width;
			const int ph = gPopupCacheH < gPopupRect.height ? gPopupCacheH : gPopupRect.height;
			const size_t popupRow = static_cast<size_t>(gPopupCacheW) * 4;
			const size_t copyBytes = static_cast<size_t>(pw) * 4;
			for (int y = 0; y < ph; ++y)
			{
				const int dy = gPopupRect.y + y;
				if (dy < 0 || dy >= gViewCacheH)
					continue;
				int x0 = 0;
				int dx0 = gPopupRect.x;
				if (dx0 < 0) { x0 = -dx0; dx0 = 0; }
				int span = pw - x0;
				if (dx0 + span > gViewCacheW)
					span = gViewCacheW - dx0;
				if (span <= 0)
					continue;
				std::memcpy(
					dstBase + static_cast<size_t>(dy) * kWikiFrameStride + static_cast<size_t>(dx0) * 4,
					gPopupCache.data() + static_cast<size_t>(y) * popupRow + static_cast<size_t>(x0) * 4,
					static_cast<size_t>(span) * 4);
				(void)copyBytes;
			}
			const int px0 = gPopupRect.x;
			const int py0 = gPopupRect.y;
			const int px1 = gPopupRect.x + gPopupRect.width;
			const int py1 = gPopupRect.y + gPopupRect.height;
			const int vx1 = ux + uw;
			const int vy1 = uy + uh;
			const int nx = px0 < ux ? px0 : ux;
			const int ny = py0 < uy ? py0 : uy;
			uw = (px1 > vx1 ? px1 : vx1) - nx;
			uh = (py1 > vy1 ? py1 : vy1) - ny;
			ux = nx;
			uy = ny;
		}

		if (ux < 0) { uw += ux; ux = 0; }
		if (uy < 0) { uh += uy; uy = 0; }
		if (ux + uw > gViewCacheW) uw = gViewCacheW - ux;
		if (uy + uh > gViewCacheH) uh = gViewCacheH - uy;
		if (uw <= 0 || uh <= 0)
		{
			ux = 0;
			uy = 0;
			uw = gViewCacheW;
			uh = gViewCacheH;
		}

		gIpc->frame_w = static_cast<uint32_t>(gViewCacheW);
		gIpc->frame_h = static_cast<uint32_t>(gViewCacheH);
		gIpc->dirty_x = static_cast<uint32_t>(ux);
		gIpc->dirty_y = static_cast<uint32_t>(uy);
		gIpc->dirty_w = static_cast<uint32_t>(uw);
		gIpc->dirty_h = static_cast<uint32_t>(uh);
		gIpc->frame_front = back;
		MemoryBarrier();
		++gIpc->frame_seq;
	}

	void UnionDirty(int* ux, int* uy, int* uw, int* uh, const cef_rect_t* dirtyRects, size_t dirtyCount, int width, int height)
	{
		*ux = 0;
		*uy = 0;
		*uw = width;
		*uh = height;
		if (dirtyCount == 0 || !dirtyRects)
			return;
		*ux = dirtyRects[0].x;
		*uy = dirtyRects[0].y;
		*uw = dirtyRects[0].width;
		*uh = dirtyRects[0].height;
		for (size_t i = 1; i < dirtyCount; ++i)
		{
			const int x0 = dirtyRects[i].x;
			const int y0 = dirtyRects[i].y;
			const int x1 = x0 + dirtyRects[i].width;
			const int y1 = y0 + dirtyRects[i].height;
			const int rx1 = *ux + *uw;
			const int ry1 = *uy + *uh;
			const int nx = x0 < *ux ? x0 : *ux;
			const int ny = y0 < *uy ? y0 : *uy;
			*uw = (x1 > rx1 ? x1 : rx1) - nx;
			*uh = (y1 > ry1 ? y1 : ry1) - ny;
			*ux = nx;
			*uy = ny;
		}
		if (*ux < 0) { *uw += *ux; *ux = 0; }
		if (*uy < 0) { *uh += *uy; *uy = 0; }
		if (*ux + *uw > width) *uw = width - *ux;
		if (*uy + *uh > height) *uh = height - *uy;
		if (*uw <= 0 || *uh <= 0)
		{
			*ux = 0;
			*uy = 0;
			*uw = width;
			*uh = height;
		}
	}

	void CEF_CALLBACK OnPopupShow(cef_render_handler_t*, cef_browser_t* browser, int show)
	{
		if (!IsActiveBrowser(browser))
			return;
		const bool wasShown = gPopupShow;
		gPopupShow = show != 0;
		if (!gPopupShow)
		{
			/* Arm one-shot swallow for the ghost mouse-up after option pick. */
			if (wasShown && gPopupRect.width > 0 && gPopupRect.height > 0)
			{
				gSwallowPopupMouseUp = true;
				gPopupSwallowRect = gPopupRect;
			}
			gPopupInvalidateOnce = false;
			gPopupCache.clear();
			gPopupCacheW = 0;
			gPopupCacheH = 0;
			gPopupRect = {};
			if (gViewCacheW > 0)
				PublishCompositedFrame(0, 0, gViewCacheW, gViewCacheH);
			return;
		}
		gSwallowPopupMouseUp = false;
		/* Request the first popup paint once — do not invalidate every frame. */
		gPopupInvalidateOnce = true;
	}

	void CEF_CALLBACK OnPopupSize(cef_render_handler_t*, cef_browser_t* browser, const cef_rect_t* rect)
	{
		if (!IsActiveBrowser(browser) || !rect)
			return;
		gPopupRect = *rect;
		if (gPopupRect.width < 0) gPopupRect.width = 0;
		if (gPopupRect.height < 0) gPopupRect.height = 0;
	}

	void CEF_CALLBACK OnPaint(
		cef_render_handler_t*, cef_browser_t* browser, cef_paint_element_type_t type,
		size_t dirtyCount, cef_rect_t const* dirtyRects, const void* buffer, int width, int height)
	{
		if (!buffer || !gFramePixels || !gIpc || width <= 0 || height <= 0)
			return;
		if (!IsActiveBrowser(browser))
			return;

		if (type == PET_POPUP)
		{
			const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
			if (bytes > 8u * 1024u * 1024u)
				return; /* absurd popup — ignore rather than allocate forever */
			gPopupCache.resize(bytes);
			std::memcpy(gPopupCache.data(), buffer, bytes);
			gPopupCacheW = width;
			gPopupCacheH = height;
			if (gPopupShow && gViewCacheW > 0)
			{
				PublishCompositedFrame(
					gPopupRect.x, gPopupRect.y,
					gPopupRect.width > 0 ? gPopupRect.width : width,
					gPopupRect.height > 0 ? gPopupRect.height : height);
			}
			return;
		}

		if (type != PET_VIEW)
			return;
		if (width > static_cast<int>(kWikiFrameMaxW) || height > static_cast<int>(kWikiFrameMaxH))
			return;

		int ux = 0, uy = 0, uw = width, uh = height;
		UnionDirty(&ux, &uy, &uw, &uh, dirtyRects, dirtyCount, width, height);

		const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
		gViewCache.resize(bytes);
		std::memcpy(gViewCache.data(), buffer, bytes);
		gViewCacheW = width;
		gViewCacheH = height;

		PublishCompositedFrame(ux, uy, uw, uh);

		if (gPopupInvalidateOnce && gPopupShow)
		{
			gPopupInvalidateOnce = false;
			cef_browser_host_t* host = browser->get_host(browser);
			if (host && host->invalidate)
			{
				host->invalidate(host, PET_POPUP);
				host->base.release(&host->base);
			}
		}
	}

}
