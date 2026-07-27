#pragma once

#include <string>

/* Downlevel Tailwind v4 / modern CSS so Chromium 103 (GW2 CEF) can paint
   modern sites. Converts oklch(), strips gradient "in oklab", rewrites
   color-mix(... transparent), and opens color-mix @supports probes. */
std::string DownlevelCss(const std::string& input);

/* Downlevel only <style>…</style> bodies inside an HTML document (do not
   touch scripts — nesting "&" rewrites would break JS). */
std::string DownlevelHtmlStyles(const std::string& html);

/* Replace YouTube <iframe> embeds (Complianz / oEmbed) with a Watch-on-YouTube
   card so CEF never loads the player (in-page Play refreshes the guide). */
std::string RewriteYoutubeEmbedsInHtml(const std::string& html);
