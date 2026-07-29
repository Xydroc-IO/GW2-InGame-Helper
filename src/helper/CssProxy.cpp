#include "CssProxy.h"

#include "CssCompat.h"

#include <atomic>
#include <cstring>
#include <new>
#include <string>

#include "include/capi/cef_response_filter_capi.h"

namespace
{
	std::string ToLower(std::string s)
	{
		for (char& c : s)
		{
			if (c >= 'A' && c <= 'Z')
				c = static_cast<char>(c - 'A' + 'a');
		}
		return s;
	}

	std::string UrlHost(const std::string& url)
	{
		const size_t scheme = url.find("://");
		size_t start = scheme == std::string::npos ? 0 : scheme + 3;
		size_t end = url.find('/', start);
		if (end == std::string::npos)
			end = url.find('?', start);
		if (end == std::string::npos)
			end = url.size();
		std::string host = url.substr(start, end - start);
		const size_t colon = host.find(':');
		if (colon != std::string::npos)
			host.resize(colon);
		return ToLower(host);
	}

	bool HostEndsWith(const std::string& host, const char* suffix)
	{
		const size_t n = std::strlen(suffix);
		return host.size() >= n && host.compare(host.size() - n, n, suffix) == 0;
	}

	struct DownlevelFilter
	{
		cef_response_filter_t filter{};
		std::atomic<int> refs{1};
		std::string input;
		std::string output;
		size_t outOffset = 0;
		bool finishedInput = false;
		bool prepared = false;
		bool isHtml = false;
		static constexpr size_t kMaxBytes = 6u * 1024u * 1024u;
	};

	void CEF_CALLBACK FilterAddRef(cef_base_ref_counted_t* base)
	{
		reinterpret_cast<DownlevelFilter*>(base)->refs.fetch_add(1, std::memory_order_relaxed);
	}

	int CEF_CALLBACK FilterRelease(cef_base_ref_counted_t* base)
	{
		auto* self = reinterpret_cast<DownlevelFilter*>(base);
		const int n = self->refs.fetch_sub(1, std::memory_order_acq_rel) - 1;
		if (n == 0)
		{
			delete self;
			return 0;
		}
		return 1;
	}

	int CEF_CALLBACK FilterHasOneRef(cef_base_ref_counted_t* base)
	{
		return reinterpret_cast<DownlevelFilter*>(base)->refs.load() == 1 ? 1 : 0;
	}

	int CEF_CALLBACK FilterHasAtLeastOneRef(cef_base_ref_counted_t* base)
	{
		return reinterpret_cast<DownlevelFilter*>(base)->refs.load() > 0 ? 1 : 0;
	}

	int CEF_CALLBACK FilterInit(cef_response_filter_t*)
	{
		return 1;
	}

	cef_response_filter_status_t CEF_CALLBACK FilterRun(
		cef_response_filter_t* self,
		void* data_in, size_t data_in_size, size_t* data_in_read,
		void* data_out, size_t data_out_size, size_t* data_out_written)
	{
		auto* f = reinterpret_cast<DownlevelFilter*>(self);
		*data_in_read = 0;
		*data_out_written = 0;

		if (data_in && data_in_size > 0)
		{
			if (f->input.size() < DownlevelFilter::kMaxBytes)
			{
				const size_t room = DownlevelFilter::kMaxBytes - f->input.size();
				const size_t n = data_in_size < room ? data_in_size : room;
				f->input.append(static_cast<const char*>(data_in), n);
				*data_in_read = n;
				if (n < data_in_size)
				{
					f->finishedInput = true;
					f->prepared = true;
					std::string body = std::move(f->input);
					f->input.clear();
					if (f->isHtml)
						body = RewriteYoutubeEmbedsInHtml(body);
					f->output = std::move(body);
				}
			}
			else
			{
				*data_in_read = data_in_size;
			}
			return RESPONSE_FILTER_NEED_MORE_DATA;
		}

		f->finishedInput = true;
		if (!f->prepared)
		{
			std::string body = std::move(f->input);
			f->input.clear();
			if (f->isHtml)
				body = RewriteYoutubeEmbedsInHtml(body);

			const bool needs = false; /* CEF 150: no CSS downlevel */
			if (!needs)
				f->output = std::move(body);
			else if (f->isHtml)
				f->output = DownlevelHtmlStyles(body);
			else
				f->output = DownlevelCss(body);
			f->prepared = true;
		}

		if (!data_out || data_out_size == 0)
			return RESPONSE_FILTER_NEED_MORE_DATA;

		if (f->outOffset >= f->output.size())
		{
			*data_out_written = 0;
			return RESPONSE_FILTER_DONE;
		}
		const size_t remain = f->output.size() - f->outOffset;
		const size_t n = remain < data_out_size ? remain : data_out_size;
		std::memcpy(data_out, f->output.data() + f->outOffset, n);
		f->outOffset += n;
		*data_out_written = n;
		return f->outOffset >= f->output.size() ? RESPONSE_FILTER_DONE : RESPONSE_FILTER_NEED_MORE_DATA;
	}
}

bool ShouldBlockUrl(const std::string& url)
{
	(void)url;
	/* Ads / consent / analytics allowed site-wide (NitroPay, AdSense, DoubleClick,
	   CookieInformation, etc.). Always false — do not reintroduce host blocklists
	   without an explicit product review. YouTube subframe cancel on Guildjen only
	   stays in OnBeforeResourceLoad — separate from ads. */
	return false;
}

bool ShouldDownlevelResponse(const std::string& url, const std::string& mime)
{
	const std::string host = UrlHost(url);
	const std::string m = ToLower(mime);
	const bool htmlLike = m.find("text/html") != std::string::npos ||
		m.find("application/xhtml") != std::string::npos;
	/* CEF 150: never buffer CSS (pass-through). Buffering large Vite/Tailwind
	   sheets delayed paint and could truncate past kMaxBytes — that broke
	   Snow Crows Alpine/Livewire (dropdowns, profile settings). */
	if (!htmlLike)
		return false;
	/* Guildjen HTML only — rewrite YouTube iframes before Complianz loads. */
	return HostEndsWith(host, "guildjen.com");
}

cef_response_filter_t* CreateCssDownlevelFilter(bool isHtml)
{
	auto* f = new (std::nothrow) DownlevelFilter();
	if (!f)
		return nullptr;
	f->isHtml = isHtml;
	f->filter.base.size = sizeof(cef_response_filter_t);
	f->filter.base.add_ref = FilterAddRef;
	f->filter.base.release = FilterRelease;
	f->filter.base.has_one_ref = FilterHasOneRef;
	f->filter.base.has_at_least_one_ref = FilterHasAtLeastOneRef;
	f->filter.init_filter = FilterInit;
	f->filter.filter = FilterRun;
	return &f->filter;
}
