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

	std::string UrlPath(const std::string& url)
	{
		const size_t scheme = url.find("://");
		size_t start = scheme == std::string::npos ? 0 : scheme + 3;
		const size_t slash = url.find('/', start);
		std::string path = slash == std::string::npos ? std::string() : url.substr(slash);
		const size_t q = path.find('?');
		if (q != std::string::npos)
			path.resize(q);
		return ToLower(path);
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
					f->output = std::move(f->input);
					f->input.clear();
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
			const bool needs = f->input.find("color-mix(") != std::string::npos ||
				f->input.find("oklch(") != std::string::npos ||
				f->input.find("@property") != std::string::npos ||
				f->input.find("dvh") != std::string::npos ||
				f->input.find("color(display") != std::string::npos ||
				f->input.find(" &") != std::string::npos;
			if (!needs)
				f->output = std::move(f->input);
			else if (f->isHtml)
				f->output = DownlevelHtmlStyles(f->input);
			else
				f->output = DownlevelCss(f->input);
			f->input.clear();
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
	const std::string host = UrlHost(url);
	const std::string path = UrlPath(url);

	if (host == "fonts.googleapis.com" || host == "fonts.gstatic.com")
		return false;

	if (HostEndsWith(host, "nitropay.com") ||
		HostEndsWith(host, "cookieinformation.com") ||
		HostEndsWith(host, "googlesyndication.com") ||
		HostEndsWith(host, "doubleclick.net") ||
		HostEndsWith(host, "google-analytics.com") ||
		HostEndsWith(host, "facebook.net") ||
		HostEndsWith(host, "facebook.com") ||
		HostEndsWith(host, "hotjar.com"))
		return true;

	if ((host == "www.google.com" || host == "google.com" || HostEndsWith(host, "gstatic.com")) &&
		path.find("recaptcha") != std::string::npos)
		return true;

	return false;
}

bool ShouldDownlevelResponse(const std::string& url, const std::string& mime)
{
	const std::string host = UrlHost(url);
	const std::string m = ToLower(mime);
	const bool cssLike = m.find("text/css") != std::string::npos ||
		m.find("stylesheet") != std::string::npos;
	const bool htmlLike = m.find("text/html") != std::string::npos ||
		m.find("application/xhtml") != std::string::npos;
	if (!cssLike && !htmlLike)
		return false;
	if (HostEndsWith(host, "google.com") || HostEndsWith(host, "gstatic.com") ||
		HostEndsWith(host, "googleapis.com") ||
		HostEndsWith(host, "duckduckgo.com") ||
		HostEndsWith(host, "hardstuck.gg") ||
		HostEndsWith(host, "snowcrows.com") ||
		HostEndsWith(host, "metabattle.com") ||
		HostEndsWith(host, "gw2efficiency.com"))
		return true;
	return false;
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
