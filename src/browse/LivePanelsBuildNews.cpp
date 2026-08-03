#include "LivePanelsBuildShared.h"

#include "Gw2Http.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <windows.h>

namespace LivePanelsBuild
{
std::string BuildNewsHtml()
{
	std::string body;
	auto build = Gw2Http::Api("/v2/build", nullptr, kLiveHttpTimeoutMs);
	long long buildId = build.ok ? JsonIntAfterKey(build.body, "id") : -1;
	body += "<section class=\"block\" id=\"build\"><div class=\"head\"><h2>Client build</h2>"
		"<p>Official API /v2/build</p></div><div class=\"body\">";
	if (buildId > 0)
	{
		body += "<p class=\"t\" style=\"font-size:1.4rem;font-weight:700;color:var(--gold)\">Build ";
		body += std::to_string(buildId);
		body += "</p>";
	}
	else
		body += "<p class=\"note\">Could not read build id (" + HtmlEscape(build.error) + ").</p>";
	body += "</div></section>\n";

	body += "<section class=\"block\" id=\"news\"><div class=\"head\"><h2>Official news</h2>"
		"<p>RSS from guildwars2.com</p></div><div class=\"body\">";
	auto feed = Gw2Http::Get("https://www.guildwars2.com/en/feed/", nullptr, kLiveHttpTimeoutMs);
	if (!feed.ok)
		feed = Gw2Http::Get("https://www.guildwars2.com/en/rss.xml", nullptr, kLiveHttpTimeoutMs);
	int newsCount = 0;
	if (feed.ok)
	{
		body += "<ul class=\"rows\">";
		size_t pos = 0;
		while (newsCount < 12)
		{
			size_t itemStart = feed.body.find("<item", pos);
			if (itemStart == std::string::npos)
				break;
			size_t itemEnd = feed.body.find("</item>", itemStart);
			if (itemEnd == std::string::npos)
				break;
			std::string item = feed.body.substr(itemStart, itemEnd - itemStart);
			size_t npos = 0;
			std::string title = ExtractTagInner(item, "title", 0, &npos);
			std::string link = ExtractTagInner(item, "link", 0, &npos);
			std::string date = ExtractTagInner(item, "pubDate", 0, &npos);
			if (title.empty())
			{
				pos = itemEnd + 7;
				continue;
			}
			body += "<li><span class=\"t\">";
			if (!link.empty())
			{
				body += "<a class=\"link\" href=\"";
				body += HtmlEscape(link);
				body += "\">";
				body += HtmlEscape(title);
				body += "</a>";
			}
			else
				body += HtmlEscape(title);
			body += "</span>";
			if (!date.empty())
			{
				body += "<span class=\"s\">";
				body += HtmlEscape(date);
				body += "</span>";
			}
			body += "</li>";
			++newsCount;
			pos = itemEnd + 7;
		}
		body += "</ul>";
	}
	if (newsCount == 0)
	{
		body += "<p class=\"note\">RSS unavailable";
		if (!feed.ok)
		{
			body += " (";
			body += HtmlEscape(feed.error);
			body += ")";
		}
		body += ". Open the official news page below.</p>";
	}
	body += "<p style=\"margin-top:12px\"><a class=\"link\" href=\"https://www.guildwars2.com/en/news/\">Official News</a>"
		" · <a class=\"link\" href=\"https://en-forum.guildwars2.com/\">Forums</a></p>";
	body += "</div></section>\n";

	body += "<section class=\"block\" id=\"updates\"><div class=\"head\"><h2>Wiki — Game updates</h2>"
		"<p>MediaWiki sections</p></div><div class=\"body\">";
	auto wiki = Gw2Http::Get(
		"https://wiki.guildwars2.com/api.php?action=parse&page=Game_updates&prop=sections&format=json&formatversion=2",
		nullptr, kLiveHttpTimeoutMs);
	int secCount = 0;
	if (wiki.ok)
	{
		body += "<ul class=\"rows\">";
		size_t p = 0;
		while (secCount < 15)
		{
			size_t line = wiki.body.find("\"line\":", p);
			if (line == std::string::npos)
				break;
			std::string lineStr = JsonStringAfterKey(wiki.body, "line", line);
			long long level = JsonIntAfterKey(wiki.body, "level", line > 80 ? line - 80 : 0);
			std::string anchor = JsonStringAfterKey(wiki.body, "anchor", line);
			p = line + 7;
			if (lineStr.empty())
				continue;
			if (level > 0 && level > 3)
				continue;
			body += "<li><span class=\"t\"><a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Game_updates";
			if (!anchor.empty())
			{
				body += "#";
				body += HtmlEscape(anchor);
			}
			body += "\">";
			body += HtmlEscape(lineStr);
			body += "</a></span></li>";
			++secCount;
		}
		body += "</ul>";
	}
	if (secCount == 0)
	{
		body += "<p class=\"note\">Could not load wiki sections. "
			"<a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Game_updates\">Open Game updates</a>.</p>";
	}
	else
	{
		body += "<p style=\"margin-top:12px\"><a class=\"link\" href=\"https://wiki.guildwars2.com/wiki/Game_updates\">Full Game updates page</a></p>";
	}
	body += "</div></section>\n";

	return BuildPage(
		"Live — News &amp; Patch Digest",
		"GW2 In-Game Helper · Live",
		"News &amp; Patch Digest",
		"Official build id, news headlines, and wiki Game updates — one place.",
		"<a href=\"#build\">Build</a>\n<a href=\"#news\">News</a>\n<a href=\"#updates\">Updates</a>",
		body);
}

} // namespace LivePanelsBuild
