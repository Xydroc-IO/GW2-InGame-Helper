#include "CssCompat.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
	struct Rgba
	{
		int r = 0;
		int g = 0;
		int b = 0;
		float a = 1.f;
	};

	float Clamp01(float x)
	{
		if (x < 0.f) return 0.f;
		if (x > 1.f) return 1.f;
		return x;
	}

	float LinearToSrgb(float x)
	{
		x = Clamp01(x);
		return x <= 0.0031308f ? 12.92f * x : 1.055f * std::pow(x, 1.f / 2.4f) - 0.055f;
	}

	Rgba OklchToRgba(float L, float C, float hDeg, float alpha)
	{
		if (L > 1.f)
			L *= 0.01f;
		const float h = hDeg * 0.017453292519943295f;
		const float a = C * std::cos(h);
		const float b = C * std::sin(h);
		const float l_ = L + 0.3963377774f * a + 0.2158037573f * b;
		const float m_ = L - 0.1055613458f * a - 0.0638541728f * b;
		const float s_ = L - 0.0894841775f * a - 1.2914855480f * b;
		const float l = l_ * l_ * l_;
		const float m = m_ * m_ * m_;
		const float s = s_ * s_ * s_;
		const float r = +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s;
		const float g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s;
		const float bl = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s;
		Rgba out;
		out.r = static_cast<int>(std::lround(LinearToSrgb(r) * 255.f));
		out.g = static_cast<int>(std::lround(LinearToSrgb(g) * 255.f));
		out.b = static_cast<int>(std::lround(LinearToSrgb(bl) * 255.f));
		out.a = Clamp01(alpha);
		return out;
	}

	bool IsDigit(char c) { return c >= '0' && c <= '9'; }
	bool IsIdentStart(char c)
	{
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-';
	}

	size_t SkipWs(const std::string& s, size_t i)
	{
		while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
			++i;
		return i;
	}

	bool ParseNumber(const std::string& s, size_t& i, float* out, bool* hadPercent)
	{
		i = SkipWs(s, i);
		size_t start = i;
		if (i < s.size() && (s[i] == '+' || s[i] == '-'))
			++i;
		bool any = false;
		while (i < s.size() && IsDigit(s[i])) { any = true; ++i; }
		if (i < s.size() && s[i] == '.')
		{
			++i;
			while (i < s.size() && IsDigit(s[i])) { any = true; ++i; }
		}
		if (!any)
			return false;
		*hadPercent = false;
		if (i < s.size() && s[i] == '%')
		{
			*hadPercent = true;
			++i;
		}
		*out = static_cast<float>(std::atof(s.substr(start, i - start - (*hadPercent ? 1 : 0)).c_str()));
		return true;
	}

	std::string FormatRgba(const Rgba& c)
	{
		char buf[64];
		if (c.a >= 0.999f)
			std::snprintf(buf, sizeof(buf), "rgb(%d,%d,%d)", c.r, c.g, c.b);
		else
			std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.4g)", c.r, c.g, c.b, static_cast<double>(c.a));
		return buf;
	}

	std::string ReplaceOklch(const std::string& input)
	{
		std::string out;
		out.reserve(input.size());
		size_t i = 0;
		while (i < input.size())
		{
			if (input.compare(i, 6, "oklch(") == 0)
			{
				size_t depth = 0;
				size_t j = i;
				for (; j < input.size(); ++j)
				{
					if (input[j] == '(') ++depth;
					else if (input[j] == ')')
					{
						--depth;
						if (depth == 0) { ++j; break; }
					}
				}
				const std::string body = input.substr(i + 6, j - i - 7);
				size_t slash = body.find('/');
				std::string numsPart = slash == std::string::npos ? body : body.substr(0, slash);
				std::string alphaPart = slash == std::string::npos ? std::string() : body.substr(slash + 1);

				size_t p = 0;
				float L = 0, C = 0, h = 0;
				bool pct = false;
				if (!ParseNumber(numsPart, p, &L, &pct))
				{
					out.append(input, i, j - i);
					i = j;
					continue;
				}
				if (!ParseNumber(numsPart, p, &C, &pct))
				{
					out.append(input, i, j - i);
					i = j;
					continue;
				}
				if (!ParseNumber(numsPart, p, &h, &pct))
				{
					out.append(input, i, j - i);
					i = j;
					continue;
				}
				float alpha = 1.f;
				if (!alphaPart.empty())
				{
					size_t ap = 0;
					bool apct = false;
					float av = 1.f;
					if (ParseNumber(alphaPart, ap, &av, &apct))
						alpha = apct ? av * 0.01f : av;
				}
				out += FormatRgba(OklchToRgba(L, C, h, alpha));
				i = j;
				continue;
			}
			out.push_back(input[i++]);
		}
		return out;
	}

	std::string StripGradientColorSpaces(std::string css)
	{
		/* Only strip color-space from gradients — never from color-mix(in srgb, …). */
		static const char* spaces[] = {
			"oklab", "oklch", "srgb", "hsl", "lab", "xyz",
		};
		for (const char* kind : {"linear-gradient(", "radial-gradient(", "conic-gradient("})
		{
			const size_t kindLen = std::strlen(kind);
			size_t pos = 0;
			while ((pos = css.find(kind, pos)) != std::string::npos)
			{
				size_t i = pos + kindLen;
				while (i < css.size() && (css[i] == ' ' || css[i] == '\t' || css[i] == '\n'))
					++i;
				if (i + 3 <= css.size() && css.compare(i, 3, "in ") == 0)
				{
					size_t sp = i + 3;
					for (const char* space : spaces)
					{
						const size_t n = std::strlen(space);
						if (css.compare(sp, n, space) == 0)
						{
							size_t end = sp + n;
							while (end < css.size() && (css[end] == ' ' || css[end] == '\t'))
								++end;
							if (end < css.size() && css[end] == ',')
								++end;
							css.erase(i, end - i);
							break;
						}
					}
				}
				pos += kindLen;
			}
		}
		return css;
	}

	void ReplaceAll(std::string& s, const std::string& from, const std::string& to)
	{
		if (from.empty())
			return;
		size_t pos = 0;
		while ((pos = s.find(from, pos)) != std::string::npos)
		{
			s.replace(pos, from.size(), to);
			pos += to.size();
		}
	}

	bool ParseHexColor(const std::string& s, Rgba* out)
	{
		if (s.size() < 4 || s[0] != '#')
			return false;
		auto hex = [](char c) -> int {
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return -1;
		};
		const std::string h = s.substr(1);
		if (h.size() == 3)
		{
			int r = hex(h[0]), g = hex(h[1]), b = hex(h[2]);
			if (r < 0 || g < 0 || b < 0) return false;
			out->r = r * 16 + r; out->g = g * 16 + g; out->b = b * 16 + b; out->a = 1.f;
			return true;
		}
		if (h.size() == 6 || h.size() == 8)
		{
			int r = hex(h[0]) * 16 + hex(h[1]);
			int g = hex(h[2]) * 16 + hex(h[3]);
			int b = hex(h[4]) * 16 + hex(h[5]);
			if (r < 0 || g < 0 || b < 0) return false;
			out->r = r; out->g = g; out->b = b; out->a = 1.f;
			if (h.size() == 8)
			{
				int a = hex(h[6]) * 16 + hex(h[7]);
				if (a < 0) return false;
				out->a = static_cast<float>(a) / 255.f;
			}
			return true;
		}
		return false;
	}

	bool ParseRgbFunc(const std::string& s, Rgba* out)
	{
		if (s.compare(0, 4, "rgba") != 0 && s.compare(0, 3, "rgb") != 0)
			return false;
		const size_t open = s.find('(');
		const size_t close = s.rfind(')');
		if (open == std::string::npos || close == std::string::npos || close <= open)
			return false;
		const std::string body = s.substr(open + 1, close - open - 1);
		int r = 0, g = 0, b = 0;
		float a = 1.f;
		if (std::sscanf(body.c_str(), "%d , %d , %d , %f", &r, &g, &b, &a) >= 3 ||
			std::sscanf(body.c_str(), "%d,%d,%d,%f", &r, &g, &b, &a) >= 3 ||
			std::sscanf(body.c_str(), "%d,%d,%d", &r, &g, &b) == 3 ||
			std::sscanf(body.c_str(), "%d , %d , %d", &r, &g, &b) == 3)
		{
			out->r = r; out->g = g; out->b = b; out->a = a;
			return true;
		}
		return false;
	}

	bool ResolveColor(const std::string& raw,
		const std::unordered_map<std::string, Rgba>& vars, Rgba* out)
	{
		std::string color = raw;
		size_t a = 0, b = color.size();
		while (a < b && (color[a] == ' ' || color[a] == '\t')) ++a;
		while (b > a && (color[b - 1] == ' ' || color[b - 1] == '\t')) --b;
		color = color.substr(a, b - a);

		if (color.compare(0, 4, "var(") == 0 && color.back() == ')')
		{
			std::string inner = color.substr(4, color.size() - 5);
			const size_t comma = inner.find(',');
			std::string name = comma == std::string::npos ? inner : inner.substr(0, comma);
			size_t ns = 0, ne = name.size();
			while (ns < ne && (name[ns] == ' ' || name[ns] == '\t')) ++ns;
			while (ne > ns && (name[ne - 1] == ' ' || name[ne - 1] == '\t')) --ne;
			name = name.substr(ns, ne - ns);
			const auto it = vars.find(name);
			if (it == vars.end())
				return false;
			*out = it->second;
			return true;
		}
		if (ParseHexColor(color, out))
			return true;
		if (ParseRgbFunc(color, out))
			return true;
		return false;
	}

	std::unordered_map<std::string, Rgba> CollectVars(const std::string& css)
	{
		std::unordered_map<std::string, Rgba> vars;
		size_t i = 0;
		while (i < css.size())
		{
			if (css[i] == '-' && i + 1 < css.size() && css[i + 1] == '-' &&
				(i == 0 || !IsIdentStart(css[i - 1])))
			{
				size_t nameStart = i;
				size_t j = i + 2;
				while (j < css.size() && (IsIdentStart(css[j]) || IsDigit(css[j])))
					++j;
				const std::string name = css.substr(nameStart, j - nameStart);
				j = SkipWs(css, j);
				if (j >= css.size() || css[j] != ':')
				{
					i = nameStart + 1;
					continue;
				}
				++j;
				j = SkipWs(css, j);
				Rgba rgba{};
				bool ok = false;
				if (j < css.size() && css[j] == '#')
				{
					size_t end = j + 1;
					while (end < css.size() &&
						((css[end] >= '0' && css[end] <= '9') ||
						 (css[end] >= 'a' && css[end] <= 'f') ||
						 (css[end] >= 'A' && css[end] <= 'F')))
						++end;
					ok = ParseHexColor(css.substr(j, end - j), &rgba);
					j = end;
				}
				else if (css.compare(j, 4, "rgba") == 0 || css.compare(j, 3, "rgb") == 0)
				{
					size_t end = j;
					int depth = 0;
					for (; end < css.size(); ++end)
					{
						if (css[end] == '(') ++depth;
						else if (css[end] == ')')
						{
							--depth;
							if (depth == 0) { ++end; break; }
						}
					}
					ok = ParseRgbFunc(css.substr(j, end - j), &rgba);
					j = end;
				}
				if (ok)
					vars[name] = rgba;
				i = j;
				continue;
			}
			++i;
		}
		return vars;
	}

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
}

std::string DownlevelCss(const std::string& input)
{
	if (input.empty())
		return input;
	/* Binary / compressed payloads — leave alone. */
	if (static_cast<unsigned char>(input[0]) == 0x1f)
		return input;

	std::string css = ReplaceOklch(input);
	css = RewriteDisplayP3(css);
	ReplaceAll(css, "@supports (color:color-mix(in lab,red,red))", "@supports (color:red)");
	/* @property gradient tokens break on Chromium ≤110 — drop them. */
	{
		std::string stripped;
		stripped.reserve(css.size());
		size_t i = 0;
		while (i < css.size())
		{
			if (css.compare(i, 9, "@property") == 0)
			{
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
				i = j;
				continue;
			}
			stripped.push_back(css[i++]);
		}
		css = std::move(stripped);
	}
	/* color-mix before stripping " in srgb" — otherwise Gemini vars stay invalid. */
	const auto vars = CollectVars(css);
	css = RewriteColorMix(css, vars);
	css = StripGradientColorSpaces(std::move(css));
	/* CEF 103: no dvh/dvw; Gemini dialogs use them. */
	ReplaceAll(css, "dvh", "vh");
	ReplaceAll(css, "dvw", "vw");
	/* Flatten leftover nesting markers that Gemini emits as top-level selectors. */
	ReplaceAll(css, " &", " ");
	ReplaceAll(css, "&>", ">");
	ReplaceAll(css, "&.", ".");
	ReplaceAll(css, "&:", ":");
	ReplaceAll(css, "&[", "[");
	return css;
}

std::string DownlevelHtmlStyles(const std::string& html)
{
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
			css.find("dvh") != std::string::npos ||
			css.find("color(display") != std::string::npos ||
			css.find(" &") != std::string::npos)
			out += DownlevelCss(css);
		else
			out += css;
		out.append(html, close, closeGt + 1 - close);
		i = closeGt + 1;
	}
	return out;
}
