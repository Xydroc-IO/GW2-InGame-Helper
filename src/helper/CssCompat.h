#pragma once

#include <string>

/* Downlevel helpers kept for reference / emergency re-enable.
   CEF Stable 150 paints oklch / color-mix / @property natively — pass-through. */
std::string DownlevelCss(const std::string& input);

/* Downlevel only <style>…</style> bodies inside an HTML document (do not
   touch scripts — nesting "&" rewrites would break JS). */
std::string DownlevelHtmlStyles(const std::string& html);

/* Replace YouTube <iframe> embeds (Complianz / oEmbed) with a Watch-on-YouTube
   card so CEF never loads the player (in-page Play refreshes the guide). */
std::string RewriteYoutubeEmbedsInHtml(const std::string& html);
