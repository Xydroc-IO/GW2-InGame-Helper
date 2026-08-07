#include "CraftingData.h"

#include "CraftingShared.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include <windows.h>

namespace CraftingDetail
{
	struct CartProject
	{
		std::string name;
		std::vector<CartItem> items;
		std::unordered_set<int> got;
	};

	static std::vector<CartProject> gProjects;
	static std::string gActive;
	static bool gLoaded = false;

	static CartProject* Find(const std::string& name)
	{
		for (CartProject& p : gProjects)
			if (p.name == name) return &p;
		return nullptr;
	}

	static CartProject* Resolve(const char* project)
	{
		if (project && project[0])
			return Find(project);
		return Find(gActive);
	}

	static std::string UniqueName(std::string base)
	{
		if (base.empty()) base = "Project";
		if (!Find(base)) return base;
		for (int n = 2; ; ++n)
		{
			std::string c = base + " " + std::to_string(n);
			if (!Find(c)) return c;
		}
	}

	static void SaveCartDisk()
	{
		const std::wstring path = ConfigFile(L"craft_cart.txt");
		if (path.empty()) return;
		std::string body = "# craft_cart v1\nACTIVE\t";
		body += gActive;
		body += "\n";
		for (const CartProject& p : gProjects)
		{
			body += "PROJECT\t";
			body += p.name;
			body += "\n";
			for (const CartItem& it : p.items)
			{
				body += "ITEM\t";
				body += std::to_string(it.id);
				body += "\t";
				body += std::to_string(it.qty);
				body += "\t";
				body += it.name;
				body += "\n";
			}
			for (int id : p.got)
			{
				body += "GOT\t";
				body += std::to_string(id);
				body += "\n";
			}
		}
		WriteUtf8File(path, body);
	}

	static void MigrateLegacyProjects()
	{
		const std::wstring path = ConfigFile(L"craft-projects.txt");
		if (path.empty()) return;
		std::string body;
		if (!ReadUtf8File(path, body)) return;
		CartProject* p = Resolve(nullptr);
		if (!p) return;
		size_t i = 0;
		while (i < body.size())
		{
			size_t e = body.find('\n', i);
			if (e == std::string::npos) e = body.size();
			std::string line = body.substr(i, e - i);
			i = e + 1;
			char name[64]{};
			char itemName[96]{};
			int id = 0;
			if (std::sscanf(line.c_str(), "%63[^\t]\t%d\t%95[^\n]", name, &id, itemName) >= 2
				&& id > 0)
			{
				CartItem it{};
				it.id = id;
				it.qty = 1;
				std::snprintf(it.name, sizeof(it.name), "%s",
					itemName[0] ? itemName : name);
				p->items.push_back(it);
			}
		}
		if (!p->items.empty())
			SaveCartDisk();
	}

	static void LoadCartDisk()
	{
		gProjects.clear();
		gActive.clear();
		const std::wstring path = ConfigFile(L"craft_cart.txt");
		std::string body;
		if (!path.empty() && ReadUtf8File(path, body))
		{
			CartProject* cur = nullptr;
			size_t i = 0;
			while (i < body.size())
			{
				size_t e = body.find('\n', i);
				if (e == std::string::npos) e = body.size();
				std::string line = body.substr(i, e - i);
				i = e + 1;
				if (!line.empty() && line.back() == '\r') line.pop_back();
				if (line.empty() || line[0] == '#') continue;
				if (line.rfind("ACTIVE\t", 0) == 0)
				{
					gActive = line.substr(7);
					continue;
				}
				if (line.rfind("PROJECT\t", 0) == 0)
				{
					CartProject p;
					p.name = line.substr(8);
					if (!p.name.empty() && !Find(p.name))
					{
						gProjects.push_back(std::move(p));
						cur = &gProjects.back();
					}
					continue;
				}
				if (!cur) continue;
				if (line.rfind("ITEM\t", 0) == 0)
				{
					int id = 0, qty = 1;
					char name[96]{};
					if (std::sscanf(line.c_str() + 5, "%d\t%d\t%95[^\n]", &id, &qty, name) >= 2
						&& id > 0)
					{
						CartItem it{};
						it.id = id;
						it.qty = qty < 1 ? 1 : qty;
						std::snprintf(it.name, sizeof(it.name), "%s", name[0] ? name : "Item");
						cur->items.push_back(it);
					}
					continue;
				}
				if (line.rfind("GOT\t", 0) == 0)
				{
					int id = 0;
					if (std::sscanf(line.c_str() + 4, "%d", &id) == 1 && id > 0)
						cur->got.insert(id);
				}
			}
		}
		if (gProjects.empty())
		{
			MigrateLegacyProjects();
			if (gProjects.empty())
				gProjects.push_back({ "My Cart", {}, {} });
		}
		if (!Find(gActive))
			gActive = gProjects.front().name;
	}

	void CartEnsureLoaded()
	{
		if (gLoaded) return;
		gLoaded = true;
		LoadCartDisk();
	}

	std::vector<std::string> CartProjectNames()
	{
		CartEnsureLoaded();
		std::vector<std::string> out;
		for (const CartProject& p : gProjects)
			out.push_back(p.name);
		return out;
	}

	const char* CartActiveName()
	{
		CartEnsureLoaded();
		return gActive.c_str();
	}

	void CartSetActive(const char* name)
	{
		CartEnsureLoaded();
		if (!name || !Find(name)) return;
		gActive = name;
		SaveCartDisk();
	}

	std::string CartNew(const char* name)
	{
		CartEnsureLoaded();
		std::string nm = UniqueName(name && name[0] ? name : "Project");
		gProjects.push_back({ nm, {}, {} });
		gActive = nm;
		SaveCartDisk();
		return nm;
	}

	bool CartRename(const char* oldName, const char* newName)
	{
		CartEnsureLoaded();
		if (!oldName || !newName || !newName[0]) return false;
		CartProject* p = Find(oldName);
		if (!p || Find(newName)) return false;
		const bool wasActive = (gActive == oldName);
		p->name = newName;
		if (wasActive) gActive = newName;
		SaveCartDisk();
		return true;
	}

	void CartDelete(const char* name)
	{
		CartEnsureLoaded();
		if (!name) return;
		auto it = std::find_if(gProjects.begin(), gProjects.end(),
			[&](const CartProject& p) { return p.name == name; });
		if (it == gProjects.end()) return;
		gProjects.erase(it);
		if (gProjects.empty())
			gProjects.push_back({ "My Cart", {}, {} });
		if (!Find(gActive))
			gActive = gProjects.front().name;
		SaveCartDisk();
	}

	std::vector<CartItem> CartItems(const char* project)
	{
		CartEnsureLoaded();
		CartProject* p = Resolve(project);
		return p ? p->items : std::vector<CartItem>{};
	}

	void CartAdd(int itemId, const char* name, int qty, const char* project)
	{
		CartEnsureLoaded();
		if (itemId <= 0 || qty <= 0) return;
		CartProject* p = Resolve(project);
		if (!p) return;
		for (CartItem& it : p->items)
		{
			if (it.id == itemId)
			{
				it.qty += qty;
				SaveCartDisk();
				return;
			}
		}
		CartItem it{};
		it.id = itemId;
		it.qty = qty;
		std::snprintf(it.name, sizeof(it.name), "%s", name && name[0] ? name : "Item");
		p->items.push_back(it);
		SaveCartDisk();
	}

	void CartSetQty(int itemId, int qty, const char* project)
	{
		CartEnsureLoaded();
		CartProject* p = Resolve(project);
		if (!p) return;
		for (size_t i = 0; i < p->items.size(); ++i)
		{
			if (p->items[i].id != itemId) continue;
			if (qty <= 0)
				p->items.erase(p->items.begin() + static_cast<std::ptrdiff_t>(i));
			else
				p->items[i].qty = qty;
			SaveCartDisk();
			return;
		}
	}

	void CartRemove(int itemId, const char* project)
	{
		CartSetQty(itemId, 0, project);
	}

	void CartClear(const char* project)
	{
		CartEnsureLoaded();
		CartProject* p = Resolve(project);
		if (!p) return;
		p->items.clear();
		p->got.clear();
		SaveCartDisk();
	}

	bool CartIsGot(int matItemId, const char* project)
	{
		CartEnsureLoaded();
		CartProject* p = Resolve(project);
		return p && p->got.count(matItemId) > 0;
	}

	void CartSetGot(int matItemId, bool on, const char* project)
	{
		CartEnsureLoaded();
		CartProject* p = Resolve(project);
		if (!p || matItemId <= 0) return;
		if (on) p->got.insert(matItemId);
		else p->got.erase(matItemId);
		SaveCartDisk();
	}

} // namespace CraftingDetail
