#include "CssCompat.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace
{
	bool AsciiIEquals(char a, char b)
	{
		if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
		if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
		return a == b;
	}

	size_t FindI(const std::string& s, const char* needle, size_t from)
	{
		const size_t n = std::strlen(needle);
		if (n == 0 || from >= s.size())
			return std::string::npos;
		for (size_t i = from; i + n <= s.size(); ++i)
		{
			bool ok = true;
			for (size_t j = 0; j < n; ++j)
			{
				if (!AsciiIEquals(s[i + j], needle[j]))
				{
					ok = false;
					break;
				}
			}
			if (ok)
				return i;
		}
		return std::string::npos;
	}

	std::string AttrValue(const std::string& tag, const char* name)
	{
		const std::string key = std::string(name) + "=\"";
		const size_t p = FindI(tag, key.c_str(), 0);
		if (p == std::string::npos)
			return {};
		const size_t start = p + key.size();
		const size_t end = tag.find('"', start);
		if (end == std::string::npos)
			return {};
		return tag.substr(start, end - start);
	}

	std::string YoutubeWatchFromTag(const std::string& tag)
	{
		std::string raw = AttrValue(tag, "data-src-cmplz");
		if (raw.empty())
			raw = AttrValue(tag, "data-src");
		if (raw.empty())
			raw = AttrValue(tag, "src");
		if (raw.empty() || raw == "about:blank")
			return "https://www.youtube.com/";
		const size_t emb = FindI(raw, "/embed/", 0);
		if (emb != std::string::npos)
		{
			size_t idStart = emb + 7;
			size_t idEnd = idStart;
			while (idEnd < raw.size())
			{
				const char c = raw[idEnd];
				if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
					(c >= '0' && c <= '9') || c == '_' || c == '-')
					++idEnd;
				else
					break;
			}
			if (idEnd > idStart)
				return "https://www.youtube.com/watch?v=" + raw.substr(idStart, idEnd - idStart);
		}
		return raw;
	}

	bool TagIsYoutubeIframe(const std::string& tag)
	{
		const size_t n = tag.size();
		std::string lower;
		lower.resize(n);
		for (size_t i = 0; i < n; ++i)
		{
			char c = tag[i];
			if (c >= 'A' && c <= 'Z')
				c = static_cast<char>(c - 'A' + 'a');
			lower[i] = c;
		}
		return lower.find("youtube") != std::string::npos ||
			lower.find("youtu.be") != std::string::npos ||
			lower.find("data-service=\"youtube\"") != std::string::npos;
	}

	std::string YoutubeCardHtml(const std::string& watch)
	{
		std::string href = watch;
		for (size_t i = 0; i < href.size(); ++i)
		{
			if (href[i] == '"')
			{
				href.replace(i, 1, "&quot;");
				i += 5;
			}
		}
		std::string out;
		out.reserve(href.size() + 420);
		out +=
			"<div data-gw2-yt=\"1\" style=\"margin:12px 0;padding:14px 16px;border:1px solid #c9a227;"
			"border-radius:6px;background:#1a1c24;color:#e8e6e3;font:14px/1.45 Segoe UI,sans-serif;\">"
			"<div style=\"font-weight:600;margin-bottom:6px;color:#e0c35a;\">YouTube video</div>"
			"<div style=\"opacity:.85;margin-bottom:10px;\">In-game playback refreshes this page. "
			"Open it in your system browser instead.</div>"
			"<a href=\"";
		out += href;
		out += "\" style=\"color:#7eb6ff;font-weight:600;\">Watch on YouTube</a></div>";
		return out;
	}
}

std::string RewriteYoutubeEmbedsInHtml(const std::string& html)
{
	if (html.size() < 32)
		return html;
	if (FindI(html, "youtube", 0) == std::string::npos &&
		FindI(html, "youtu.be", 0) == std::string::npos)
		return html;

	std::string out;
	out.reserve(html.size());
	size_t i = 0;
	while (i < html.size())
	{
		const size_t start = FindI(html, "<iframe", i);
		if (start == std::string::npos)
		{
			out.append(html, i, std::string::npos);
			break;
		}
		out.append(html, i, start - i);
		const size_t gt = html.find('>', start);
		if (gt == std::string::npos)
		{
			out.append(html, start, std::string::npos);
			break;
		}
		size_t end = gt + 1;
		/* Prefer matching </iframe> when present. */
		const size_t close = FindI(html, "</iframe>", gt + 1);
		if (close != std::string::npos && close < start + 4000)
			end = close + 9;
		const std::string tag = html.substr(start, gt + 1 - start);
		if (TagIsYoutubeIframe(tag))
		{
			out += YoutubeCardHtml(YoutubeWatchFromTag(tag));
			i = end;
		}
		else
		{
			out.append(html, start, end - start);
			i = end;
		}
	}
	return out;
}

