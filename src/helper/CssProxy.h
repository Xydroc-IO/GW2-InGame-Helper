#pragma once

#include <cstring>
#include <string>

#include "include/capi/cef_resource_handler_capi.h"
#include "include/capi/cef_response_filter_capi.h"

/* Optional URL cancel list. Currently always false — ads/trackers allowed. */
bool ShouldBlockUrl(const std::string& url);

/* True when response body should be CSS-downleveled via a CEF filter
   (Google/Gemini HTML inline styles, modern site stylesheets). */
bool ShouldDownlevelResponse(const std::string& url, const std::string& mime);

/* WinHTTP CSS proxy disabled (Wine/Proton). Always returns nullptr.
   Styles are fixed via response filter + BootJs using CEF's network. */
inline cef_resource_handler_t* CreateCssProxyHandler(const std::string&)
{
	return nullptr;
}

/* Heap-allocated response filter; CEF owns the refcount after return.
   Pass isHtml=true for text/html so only <style> blocks are rewritten. */
cef_response_filter_t* CreateCssDownlevelFilter(bool isHtml);
