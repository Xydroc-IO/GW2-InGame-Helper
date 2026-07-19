#pragma once

#include <cstring>
#include <string>

#include "include/capi/cef_resource_handler_capi.h"
#include "include/capi/cef_request_capi.h"

/* True for ad / consent / tracker hosts that slow CEF 103 to a crawl. */
bool ShouldBlockUrl(const std::string& url);

/* True when this request should be proxied and CSS-downleveled. */
bool ShouldProxyCss(const std::string& url);

/* Heap-allocated handler; CEF owns the refcount after return. */
cef_resource_handler_t* CreateCssProxyHandler(const std::string& url);
