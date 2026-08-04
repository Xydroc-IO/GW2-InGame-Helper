#include "CssCompat.h"

#include <string>

std::string DownlevelCss(const std::string& input)
{
	/* CEF 150+: native modern CSS — no oklch/color-mix rewrite. */
	return input;
#if 0
	/* Legacy CEF 103 downlevel kept for reference. */
	if (input.empty())
		return input;
	if (static_cast<unsigned char>(input[0]) == 0x1f)
		return input;

	std::string css = ReplaceOklch(input);
	css = RewriteDisplayP3(css);
	ReplaceAll(css, "@supports (color:color-mix(in lab,red,red))", "@supports (color:red)");
	css = StripPropertyKeepInitials(std::move(css));
	css = RewriteContainerQueries(std::move(css));
	const auto vars = CollectVars(css);
	css = RewriteColorMix(css, vars);
	css = StripGradientColorSpaces(std::move(css));
	{
		std::string flat;
		flat.reserve(css.size());
		for (size_t i = 0; i < css.size();)
		{
			if (i > 0 && IsDigit(css[i - 1]) && css.compare(i, 3, "dvh") == 0)
			{
				flat += "vh";
				i += 3;
				continue;
			}
			if (i > 0 && IsDigit(css[i - 1]) && css.compare(i, 3, "dvw") == 0)
			{
				flat += "vw";
				i += 3;
				continue;
			}
			flat.push_back(css[i++]);
		}
		css = std::move(flat);
	}
	ReplaceAll(css, " &", " ");
	ReplaceAll(css, "&>", ">");
	ReplaceAll(css, "&.", ".");
	ReplaceAll(css, "&:", ":");
	ReplaceAll(css, "&[", "[");
	return css;
#endif
}

std::string DownlevelHtmlStyles(const std::string& html)
{
	/* CEF 150+: leave inline <style> bodies alone. */
	return html;
#if 0
	if (html.empty())
		return html;
	std::string out;
	out.reserve(html.size());
	size_t i = 0;
	while (i < html.size())
	{
		/* Case-insensitive <style ...> … </style> */
		const size_t open = html.find("<style", i);
		if (open == std::string::npos)
		{
			out.append(html, i, std::string::npos);
			break;
		}
		/* Verify tag boundary */
		size_t tagEnd = open + 6;
		if (tagEnd < html.size() && html[tagEnd] != '>' && html[tagEnd] != ' ' &&
			html[tagEnd] != '\t' && html[tagEnd] != '\n' && html[tagEnd] != '/')
		{
			out.append(html, i, open + 6 - i);
			i = open + 6;
			continue;
		}
		const size_t gt = html.find('>', open);
		if (gt == std::string::npos)
		{
			out.append(html, i, std::string::npos);
			break;
		}
		size_t close = html.find("</style", gt + 1);
		if (close == std::string::npos)
			close = html.find("</STYLE", gt + 1);
		if (close == std::string::npos)
		{
			out.append(html, i, std::string::npos);
			break;
		}
		const size_t closeGt = html.find('>', close);
		if (closeGt == std::string::npos)
		{
			out.append(html, i, std::string::npos);
			break;
		}
		out.append(html, i, gt + 1 - i);
		const std::string css = html.substr(gt + 1, close - (gt + 1));
		if (css.find("color-mix(") != std::string::npos ||
			css.find("oklch(") != std::string::npos ||
			css.find("@property") != std::string::npos ||
			css.find("@container") != std::string::npos ||
			css.find("dvh") != std::string::npos ||
			css.find("dvw") != std::string::npos ||
			css.find("color(display") != std::string::npos ||
			css.find(" &") != std::string::npos)
			out += DownlevelCss(css);
		else
			out += css;
		out.append(html, close, closeGt + 1 - close);
		i = closeGt + 1;
	}
	return out;
#endif
}

