/* Legacy CSS color-mix / P3 / @property / container rewrites (reference). */
#include "CssCompatInternal.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

namespace CssCompatDetail
{
std::string RewriteColorMix(const std::string& css,
	const std::unordered_map<std::string, Rgba>& vars)
{
	std::string out;
	out.reserve(css.size());
	size_t i = 0;
	while (i < css.size())
	{
		if (css.compare(i, 10, "color-mix(") == 0)
		{
			size_t depth = 0;
			size_t j = i;
			for (; j < css.size(); ++j)
			{
				if (css[j] == '(') ++depth;
				else if (css[j] == ')')
				{
					--depth;
					if (depth == 0) { ++j; break; }
				}
			}
			const std::string full = css.substr(i, j - i);
			/* color-mix(in SPACE, COLOR PCT%, transparent) */
			const size_t comma1 = full.find(',');
			if (comma1 != std::string::npos)
			{
				const size_t transparent = full.rfind("transparent");
				if (transparent != std::string::npos && transparent > comma1)
				{
					std::string mid = full.substr(comma1 + 1, transparent - comma1 - 1);
					/* strip trailing comma/ws */
					while (!mid.empty() && (mid.back() == ' ' || mid.back() == '\t' || mid.back() == ','))
						mid.pop_back();
					/* find trailing N% */
					size_t pctPos = mid.find_last_of('%');
					if (pctPos != std::string::npos && pctPos > 0)
					{
						size_t numEnd = pctPos;
						size_t numStart = numEnd;
						while (numStart > 0 && (IsDigit(mid[numStart - 1]) || mid[numStart - 1] == '.'))
							--numStart;
						if (numStart < numEnd)
						{
							const float pct = static_cast<float>(std::atof(mid.substr(numStart, numEnd - numStart).c_str()));
							std::string color = mid.substr(0, numStart);
							while (!color.empty() && (color.back() == ' ' || color.back() == '\t'))
								color.pop_back();
							/* reject nested color-mix for now */
							if (color.find("color-mix(") == std::string::npos)
							{
								Rgba base{};
								if (ResolveColor(color, vars, &base))
								{
									Rgba mixed = base;
									mixed.a = Clamp01(base.a * (pct * 0.01f));
									out += FormatRgba(mixed);
									i = j;
									continue;
								}
							}
						}
					}
				}
			}
			out += full;
			i = j;
			continue;
		}
		out.push_back(css[i++]);
	}
	return out;
}

std::string RewriteDisplayP3(const std::string& css)
{
	/* color(display-p3 R G B[/A]) with 0–1 channels → rgba (approx sRGB). */
	std::string out;
	out.reserve(css.size());
	size_t i = 0;
	const char* prefix = "color(display-p3";
	const size_t plen = 16;
	while (i < css.size())
	{
		if (css.compare(i, plen, prefix) == 0)
		{
			size_t depth = 0;
			size_t j = i;
			for (; j < css.size(); ++j)
			{
				if (css[j] == '(') ++depth;
				else if (css[j] == ')')
				{
					--depth;
					if (depth == 0) { ++j; break; }
				}
			}
			const std::string full = css.substr(i, j - i);
			/* color(display-p3 0 0 0/6%) or color(display-p3 1 1 1/.72) */
			float r = 0.f, g = 0.f, b = 0.f, a = 1.f;
			const size_t open = full.find('3'); /* end of p3 */
			std::string body = full.substr(open + 1);
			if (!body.empty() && body.back() == ')')
				body.pop_back();
			/* trim */
			size_t a0 = 0, b0 = body.size();
			while (a0 < b0 && (body[a0] == ' ' || body[a0] == '\t')) ++a0;
			while (b0 > a0 && (body[b0 - 1] == ' ' || body[b0 - 1] == '\t')) --b0;
			body = body.substr(a0, b0 - a0);
			/* split alpha on last '/' */
			std::string rgbPart = body;
			std::string alphaPart;
			const size_t slash = body.find_last_of('/');
			if (slash != std::string::npos)
			{
				rgbPart = body.substr(0, slash);
				alphaPart = body.substr(slash + 1);
			}
			float rv = 0, gv = 0, bv = 0;
			if (std::sscanf(rgbPart.c_str(), "%f %f %f", &rv, &gv, &bv) == 3)
			{
				r = Clamp01(rv); g = Clamp01(gv); b = Clamp01(bv);
				if (!alphaPart.empty())
				{
					size_t ap = 0;
					bool apct = false;
					float av = 1.f;
					if (ParseNumber(alphaPart, ap, &av, &apct))
						a = Clamp01(apct ? av * 0.01f : av);
				}
				char buf[64];
				std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%g)",
					static_cast<int>(std::lround(r * 255.f)),
					static_cast<int>(std::lround(g * 255.f)),
					static_cast<int>(std::lround(b * 255.f)),
					static_cast<double>(a));
				out += buf;
				i = j;
				continue;
			}
			out += full;
			i = j;
			continue;
		}
		out.push_back(css[i++]);
	}
	return out;
}

std::string StripPropertyKeepInitials(std::string css)
{
	/* @property is ignored / buggy on Chromium ≤110. Keep initial-value as
	   ordinary custom props so Tailwind v4 utilities still resolve. */
	std::string fallbacks;
	std::string stripped;
	stripped.reserve(css.size());
	size_t i = 0;
	while (i < css.size())
	{
		if (css.compare(i, 9, "@property") == 0)
		{
			size_t nameStart = i + 9;
			while (nameStart < css.size() && (css[nameStart] == ' ' || css[nameStart] == '\t' ||
				css[nameStart] == '\n' || css[nameStart] == '\r'))
				++nameStart;
			size_t nameEnd = nameStart;
			while (nameEnd < css.size() && css[nameEnd] != '{' && css[nameEnd] != ' ' &&
				css[nameEnd] != '\t' && css[nameEnd] != '\n')
				++nameEnd;
			const std::string propName = css.substr(nameStart, nameEnd - nameStart);
			const size_t brace = css.find('{', i);
			if (brace == std::string::npos)
			{
				stripped.push_back(css[i++]);
				continue;
			}
			size_t depth = 0, j = brace;
			for (; j < css.size(); ++j)
			{
				if (css[j] == '{') ++depth;
				else if (css[j] == '}')
				{
					--depth;
					if (depth == 0) { ++j; break; }
				}
			}
			const std::string body = css.substr(brace + 1, (j > brace + 1) ? (j - brace - 2) : 0);
			const size_t iv = body.find("initial-value:");
			if (iv != std::string::npos && propName.size() > 2 && propName[0] == '-' && propName[1] == '-')
			{
				size_t vs = iv + 14;
				while (vs < body.size() && (body[vs] == ' ' || body[vs] == '\t')) ++vs;
				size_t ve = vs;
				while (ve < body.size() && body[ve] != ';' && body[ve] != '}') ++ve;
				std::string val = body.substr(vs, ve - vs);
				while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
				if (!val.empty() && val != "initial")
					fallbacks += propName + ":" + val + ";";
			}
			i = j;
			continue;
		}
		stripped.push_back(css[i++]);
	}
	if (!fallbacks.empty())
		return "*,:before,:after,::backdrop{" + fallbacks + "}" + stripped;
	return stripped;
}

std::string RewriteContainerQueries(std::string css)
{
	/* CEF 103 has no @container — approximate with @media so peek/modal
	   width utilities still apply at the viewport size. */
	size_t pos = 0;
	while ((pos = css.find("@container", pos)) != std::string::npos)
	{
		size_t i = pos + 10;
		while (i < css.size() && (css[i] == ' ' || css[i] == '\t' || css[i] == '\n' || css[i] == '\r'))
			++i;
		/* Named container definition: @container\/modal{container:...} — drop. */
		if (i < css.size() && css[i] == '\\')
		{
			const size_t brace = css.find('{', i);
			if (brace == std::string::npos)
			{
				pos = i;
				continue;
			}
			size_t depth = 0, j = brace;
			for (; j < css.size(); ++j)
			{
				if (css[j] == '{') ++depth;
				else if (css[j] == '}')
				{
					--depth;
					if (depth == 0) { ++j; break; }
				}
			}
			css.erase(pos, j - pos);
			continue;
		}
		const size_t paren = css.find('(', i);
		const size_t brace = css.find('{', i);
		if (paren == std::string::npos || brace == std::string::npos || paren > brace)
		{
			pos = i;
			continue;
		}
		const size_t closeParen = css.find(')', paren);
		if (closeParen == std::string::npos || closeParen > brace)
		{
			pos = i;
			continue;
		}
		const std::string cond = css.substr(paren, closeParen - paren + 1);
		const std::string repl = "@media " + cond + " ";
		css.replace(pos, brace - pos, repl);
		pos += repl.size();
	}
	return css;
}
}
