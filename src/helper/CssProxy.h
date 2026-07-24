#pragma once

#include <cstring>
#include <string>

#include "include/capi/cef_resource_handler_capi.h"
#include "include/capi/cef_request_capi.h"
#include "include/capi/cef_response_filter_capi.h"

/* True for ad / consent / tracker hosts that slow CEF 103 to a crawl. */
bool ShouldBlockUrl(const std::string& url);

/* True when this request should be proxied and CSS-downleveled. */
bool ShouldProxyCss(const std::string& url);

/* True when response body should be CSS-downleveled via a CEF filter
   (Google/Gemini HTML inline styles, modern site stylesheets). */
bool ShouldDownlevelResponse(const std::string& url, const std::string& mime);

/* Heap-allocated handler; CEF owns the refcount after return. */
cef_resource_handler_t* CreateCssProxyHandler(const std::string& url);

/* Heap-allocated response filter; CEF owns the refcount after return.
   Pass isHtml=true for text/html so only <style> blocks are rewritten. */
cef_response_filter_t* CreateCssDownlevelFilter(bool isHtml);

