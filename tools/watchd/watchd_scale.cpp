#include "watchd_internal.h"

#include <spa/param/video/raw.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <vector>

namespace WatchdDetail
{
	namespace
	{
		constexpr int kTargetFps = 60;
		std::chrono::steady_clock::time_point gLastPub{};

		bool PaceOk()
		{
			using clock = std::chrono::steady_clock;
			const auto now = clock::now();
			if (gLastPub.time_since_epoch().count() != 0)
			{
				const auto minGap = std::chrono::milliseconds(1000 / kTargetFps);
				if (now - gLastPub < minGap)
					return false;
			}
			gLastPub = now;
			return true;
		}

		void PixelToBgra(int fmt, const uint8_t* s, uint8_t* d)
		{
			switch (fmt)
			{
			case SPA_VIDEO_FORMAT_BGRx:
			case SPA_VIDEO_FORMAT_BGRA:
				d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
				break;
			case SPA_VIDEO_FORMAT_RGBx:
			case SPA_VIDEO_FORMAT_RGBA:
				d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = 255;
				break;
			case SPA_VIDEO_FORMAT_xRGB:
			case SPA_VIDEO_FORMAT_ARGB:
				d[0] = s[3]; d[1] = s[2]; d[2] = s[1]; d[3] = 255;
				break;
			case SPA_VIDEO_FORMAT_xBGR:
			case SPA_VIDEO_FORMAT_ABGR:
				d[0] = s[1]; d[1] = s[2]; d[2] = s[3]; d[3] = 255;
				break;
			default:
				d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = 255;
				break;
			}
		}
	}

	void PublishSpaFrame(uint32_t srcW, uint32_t srcH, uint32_t srcStride,
		const uint8_t* src, int spaVideoFormat)
	{
		if (!src || srcW < 2 || srcH < 2 || srcStride < srcW * 4)
			return;
		if (!PaceOk())
			return;

		uint32_t dstW = srcW;
		uint32_t dstH = srcH;
		if (dstW > WatchProto::kMaxW || dstH > WatchProto::kMaxH)
		{
			const float sx = static_cast<float>(WatchProto::kMaxW) / static_cast<float>(srcW);
			const float sy = static_cast<float>(WatchProto::kMaxH) / static_cast<float>(srcH);
			const float s = (std::min)(sx, sy);
			dstW = (std::max)(2u, static_cast<uint32_t>(srcW * s));
			dstH = (std::max)(2u, static_cast<uint32_t>(srcH * s));
		}

		thread_local std::vector<uint8_t> scratch;
		const size_t need = static_cast<size_t>(dstW) * dstH * 4;
		if (scratch.size() < need)
			scratch.resize(need);

		for (uint32_t y = 0; y < dstH; ++y)
		{
			const uint32_t sy = (srcH <= 1) ? 0 : (y * (srcH - 1)) / (dstH - 1);
			const uint8_t* srow = src + static_cast<size_t>(sy) * srcStride;
			uint8_t* drow = scratch.data() + static_cast<size_t>(y) * dstW * 4;
			for (uint32_t x = 0; x < dstW; ++x)
			{
				const uint32_t sx = (srcW <= 1) ? 0 : (x * (srcW - 1)) / (dstW - 1);
				PixelToBgra(spaVideoFormat, srow + static_cast<size_t>(sx) * 4, drow + x * 4);
			}
		}
		PublishBgra(dstW, dstH, scratch.data(), dstW * 4);
	}
}
