#pragma once

#include <string>

/* Downlevel Tailwind v4 / modern CSS so Chromium 103 (GW2 CEF) can paint
   snowcrows.com. Converts oklch(), strips gradient "in oklab", rewrites
   color-mix(... transparent), and opens color-mix @supports probes. */
std::string DownlevelCss(const std::string& input);

/* Downlevel only <style>…</style> bodies inside an HTML document (do not
   touch scripts — nesting "&" rewrites would break JS). */
std::string DownlevelHtmlStyles(const std::string& html);
