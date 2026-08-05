#include "SitesLoadInternal.h"

#include <cstring>
#include <string>
#include <vector>

namespace SitesDetail
{
	const char* kSitesStamp = "s2213";

	std::vector<SiteOwned> gOwned;
	std::vector<SiteDef> gDefs;
	std::unordered_map<std::string, SectionList> gSections;

	SiteOwned gFallbackOwned;
	SiteDef gFallbackDef{};
	const char* gFallbackBrowse[1]{};

	void SkipWs(const char*& p, const char* end)
	{
		while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
			++p;
	}

	bool ParseString(const char*& p, const char* end, std::string& out)
	{
		SkipWs(p, end);
		if (p >= end || *p != '"')
			return false;
		++p;
		out.clear();
		while (p < end)
		{
			const char c = *p++;
			if (c == '"')
				return true;
			if (c == '\\' && p < end)
			{
				const char e = *p++;
				switch (e)
				{
				case '"':
				case '\\':
				case '/':
					out.push_back(e);
					break;
				case 'n':
					out.push_back('\n');
					break;
				case 'r':
					out.push_back('\r');
					break;
				case 't':
					out.push_back('\t');
					break;
				case 'u':
					/* Skip \uXXXX — keep as '?' for catalog text. */
					for (int i = 0; i < 4 && p < end; ++i)
						++p;
					out.push_back('?');
					break;
				default:
					out.push_back(e);
					break;
				}
			}
			else
				out.push_back(c);
		}
		return false;
	}

	bool Expect(const char*& p, const char* end, char ch)
	{
		SkipWs(p, end);
		if (p >= end || *p != ch)
			return false;
		++p;
		return true;
	}

	void SkipValue(const char*& p, const char* end)
	{
		SkipWs(p, end);
		if (p >= end)
			return;
		if (*p == '"')
		{
			std::string tmp;
			ParseString(p, end, tmp);
			return;
		}
		if (*p == '{')
		{
			++p;
			int depth = 1;
			while (p < end && depth > 0)
			{
				if (*p == '"')
				{
					std::string tmp;
					ParseString(p, end, tmp);
					continue;
				}
				if (*p == '{')
					++depth;
				else if (*p == '}')
					--depth;
				++p;
			}
			return;
		}
		if (*p == '[')
		{
			++p;
			int depth = 1;
			while (p < end && depth > 0)
			{
				if (*p == '"')
				{
					std::string tmp;
					ParseString(p, end, tmp);
					continue;
				}
				if (*p == '[')
					++depth;
				else if (*p == ']')
					--depth;
				++p;
			}
			return;
		}
		/* null / true / false / number */
		while (p < end && *p != ',' && *p != '}' && *p != ']' &&
			*p != ' ' && *p != '\n' && *p != '\r' && *p != '\t')
			++p;
	}

	bool ParseStringOrNull(const char*& p, const char* end, std::string& out, bool& isNull)
	{
		SkipWs(p, end);
		isNull = false;
		if (p + 4 <= end && std::strncmp(p, "null", 4) == 0)
		{
			p += 4;
			isNull = true;
			out.clear();
			return true;
		}
		return ParseString(p, end, out);
	}

	bool ParseBrowsePath(const char*& p, const char* end, SiteOwned& site)
	{
		SkipWs(p, end);
		if (p >= end || *p != '[')
			return false;
		++p;
		site.browsePathCount = 0;
		while (true)
		{
			SkipWs(p, end);
			if (p < end && *p == ']')
			{
				++p;
				return true;
			}
			std::string part;
			if (!ParseString(p, end, part))
				return false;
			if (site.browsePathCount < kMaxBrowsePath && !part.empty())
			{
				site.browsePath[site.browsePathCount] = std::move(part);
				++site.browsePathCount;
			}
			SkipWs(p, end);
			if (p < end && *p == ',')
			{
				++p;
				continue;
			}
			if (p < end && *p == ']')
			{
				++p;
				return true;
			}
			return false;
		}
	}

	bool ParseSiteObject(const char*& p, const char* end, SiteOwned& site)
	{
		if (!Expect(p, end, '{'))
			return false;
		site = SiteOwned{};
		while (true)
		{
			SkipWs(p, end);
			if (p < end && *p == '}')
			{
				++p;
				return !site.id.empty() && !site.category.empty() &&
					!site.label.empty() && !site.title.empty() && !site.homeUrl.empty();
			}
			std::string key;
			if (!ParseString(p, end, key) || !Expect(p, end, ':'))
				return false;
			if (key == "id")
			{
				if (!ParseString(p, end, site.id))
					return false;
			}
			else if (key == "category")
			{
				if (!ParseString(p, end, site.category))
					return false;
			}
			else if (key == "label")
			{
				if (!ParseString(p, end, site.label))
					return false;
			}
			else if (key == "title")
			{
				if (!ParseString(p, end, site.title))
					return false;
			}
			else if (key == "homeUrl")
			{
				if (!ParseString(p, end, site.homeUrl))
					return false;
			}
			else if (key == "searchUrlPrefix")
			{
				bool isNull = false;
				if (!ParseStringOrNull(p, end, site.searchPrefix, isNull))
					return false;
				site.hasSearchPrefix = !isNull && !site.searchPrefix.empty();
			}
			else if (key == "searchUrlSuffix")
			{
				bool isNull = false;
				if (!ParseStringOrNull(p, end, site.searchSuffix, isNull))
					return false;
				site.hasSearchSuffix = !isNull && !site.searchSuffix.empty();
			}
			else if (key == "browsePath")
			{
				if (!ParseBrowsePath(p, end, site))
					return false;
			}
			else
				SkipValue(p, end);

			SkipWs(p, end);
			if (p < end && *p == ',')
			{
				++p;
				continue;
			}
			if (p < end && *p == '}')
			{
				++p;
				return !site.id.empty() && !site.category.empty() &&
					!site.label.empty() && !site.title.empty() && !site.homeUrl.empty();
			}
			return false;
		}
	}

	bool ParseStringArray(const char*& p, const char* end, std::vector<std::string>& out)
	{
		out.clear();
		if (!Expect(p, end, '['))
			return false;
		while (true)
		{
			SkipWs(p, end);
			if (p < end && *p == ']')
			{
				++p;
				return true;
			}
			std::string s;
			if (!ParseString(p, end, s))
				return false;
			out.push_back(std::move(s));
			SkipWs(p, end);
			if (p < end && *p == ',')
			{
				++p;
				continue;
			}
			if (p < end && *p == ']')
			{
				++p;
				return true;
			}
			return false;
		}
	}

	bool ParseBrowseSections(const char*& p, const char* end)
	{
		if (!Expect(p, end, '{'))
			return false;
		gSections.clear();
		while (true)
		{
			SkipWs(p, end);
			if (p < end && *p == '}')
			{
				++p;
				return true;
			}
			std::string cat;
			if (!ParseString(p, end, cat) || !Expect(p, end, ':'))
				return false;
			SectionList list;
			if (!ParseStringArray(p, end, list.names))
				return false;
			list.ptrs.reserve(list.names.size());
			for (const std::string& n : list.names)
				list.ptrs.push_back(n.c_str());
			gSections.emplace(std::move(cat), std::move(list));
			/* Fix pointers after map move — rebuild below. */
			SkipWs(p, end);
			if (p < end && *p == ',')
			{
				++p;
				continue;
			}
			if (p < end && *p == '}')
			{
				++p;
				/* Rebuild ptrs after all insertions (string addresses stable in map values). */
				for (auto& kv : gSections)
				{
					kv.second.ptrs.clear();
					kv.second.ptrs.reserve(kv.second.names.size());
					for (const std::string& n : kv.second.names)
						kv.second.ptrs.push_back(n.c_str());
				}
				return true;
			}
			return false;
		}
	}

	bool ParseRoot(const std::string& json)
	{
		const char* p = json.data();
		const char* end = json.data() + json.size();
		if (!Expect(p, end, '{'))
			return false;

		gOwned.clear();
		gOwned.reserve(2800);
		gSections.clear();
		bool sawSites = false;

		while (true)
		{
			SkipWs(p, end);
			if (p < end && *p == '}')
			{
				++p;
				break;
			}
			std::string key;
			if (!ParseString(p, end, key) || !Expect(p, end, ':'))
				return false;
			if (key == "browseSections")
			{
				if (!ParseBrowseSections(p, end))
					return false;
			}
			else if (key == "sites")
			{
				if (!Expect(p, end, '['))
					return false;
				sawSites = true;
				while (true)
				{
					SkipWs(p, end);
					if (p < end && *p == ']')
					{
						++p;
						break;
					}
					SiteOwned site;
					if (!ParseSiteObject(p, end, site))
						return false;
					gOwned.push_back(std::move(site));
					SkipWs(p, end);
					if (p < end && *p == ',')
					{
						++p;
						continue;
					}
					if (p < end && *p == ']')
					{
						++p;
						break;
					}
					return false;
				}
			}
			else
				SkipValue(p, end);

			SkipWs(p, end);
			if (p < end && *p == ',')
			{
				++p;
				continue;
			}
			if (p < end && *p == '}')
			{
				++p;
				break;
			}
			return false;
		}

		if (!sawSites || gOwned.empty())
			return false;

		gDefs.clear();
		gDefs.resize(gOwned.size());
		for (size_t i = 0; i < gOwned.size(); ++i)
		{
			SiteOwned& o = gOwned[i];
			SiteDef& d = gDefs[i];
			d.id = o.id.c_str();
			d.category = o.category.c_str();
			d.label = o.label.c_str();
			d.title = o.title.c_str();
			d.homeUrl = o.homeUrl.c_str();
			d.searchUrlPrefix = o.hasSearchPrefix ? o.searchPrefix.c_str() : nullptr;
			d.searchUrlSuffix = o.hasSearchSuffix ? o.searchSuffix.c_str() : nullptr;
			for (int j = 0; j < o.browsePathCount; ++j)
				o.browsePtrs[j] = o.browsePath[j].c_str();
			d.browsePath = o.browsePathCount > 0 ? o.browsePtrs : nullptr;
			d.browsePathCount = o.browsePathCount;
		}
		gSites = gDefs.data();
		gSiteCount = static_cast<int>(gDefs.size());
		return true;
	}
}
