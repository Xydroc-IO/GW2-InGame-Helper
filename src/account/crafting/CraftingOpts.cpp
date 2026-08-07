#include "CraftingData.h"

#include "CraftingShared.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace CraftingDetail
{
	void LoadCraftOpts()
	{
		const std::wstring path = ConfigFile(L"craft_opts.txt");
		if (path.empty()) return;
		std::string body;
		if (!ReadUtf8File(path, body)) return;
		size_t i = 0;
		while (i < body.size())
		{
			size_t e = body.find('\n', i);
			if (e == std::string::npos) e = body.size();
			std::string line = body.substr(i, e - i);
			i = e + 1;
			if (!line.empty() && line.back() == '\r') line.pop_back();
			int v = 0;
			if (std::sscanf(line.c_str(), "useOwn=%d", &v) == 1)
				gOpts.useOwnMaterials = v != 0;
			else if (std::sscanf(line.c_str(), "craftSub=%d", &v) == 1)
				gOpts.craftSubComponents = v != 0;
			else if (std::sscanf(line.c_str(), "groupBy=%d", &v) == 1)
				gOpts.groupByItem = v != 0;
		}
	}

	void SaveCraftOpts()
	{
		const std::wstring path = ConfigFile(L"craft_opts.txt");
		if (path.empty()) return;
		char body[128];
		std::snprintf(body, sizeof(body),
			"useOwn=%d\ncraftSub=%d\ngroupBy=%d\n",
			gOpts.useOwnMaterials ? 1 : 0,
			gOpts.craftSubComponents ? 1 : 0,
			gOpts.groupByItem ? 1 : 0);
		WriteUtf8File(path, body);
	}

} // namespace CraftingDetail
