#include "PathingLuaInternal.h"
#include "PathingLua.h"

#include "HelperTheme.h"
#include "PadNav.h"

#include "imgui/imgui.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace PathingLuaDetail
{
	namespace
	{
		struct MenuItem
		{
			int         id = 0;
			int         parentId = 0;
			std::string name;
			int         onClickRef = LUA_NOREF;
			bool        canCheck = false;
			bool        checked = false;
			std::string tooltip;
		};

		std::mutex gMenuMu;
		std::vector<MenuItem> gItems;
		int gNextId = 1;
		lua_State* gMenuL = nullptr;

		int Menu_Add(lua_State* L);
		int Menu_Remove(lua_State* L);

		void UnrefItem(lua_State* L, MenuItem& it)
		{
			if (L && it.onClickRef != LUA_NOREF)
			{
				luaL_unref(L, LUA_REGISTRYINDEX, it.onClickRef);
				it.onClickRef = LUA_NOREF;
			}
		}

		void FireClick(lua_State* L, MenuItem& it)
		{
			if (!L || it.onClickRef == LUA_NOREF)
				return;
			lua_rawgeti(L, LUA_REGISTRYINDEX, it.onClickRef);
			PushMenuRef(L, it.id);
			if (lua_pcall(L, 1, 0, 0) != LUA_OK)
			{
				LogLuaError(L, "Menu.Add callback");
				lua_pop(L, 1);
			}
		}

		int Menu_SetChecked(lua_State* L)
		{
			/* __newindex on menu ref: menu.Checked = bool */
			lua_getfield(L, 1, "Id");
			const int id = static_cast<int>(luaL_optinteger(L, -1, 0));
			lua_pop(L, 1);
			const char* key = luaL_checkstring(L, 2);
			if (!key || std::strcmp(key, "Checked") != 0)
				return 0;
			const bool checked = lua_toboolean(L, 3) != 0;
			std::lock_guard<std::mutex> lock(gMenuMu);
			for (auto& it : gItems)
			{
				if (it.id == id)
				{
					it.checked = checked;
					break;
				}
			}
			return 0;
		}

		int Menu_GetChecked(lua_State* L)
		{
			lua_getfield(L, 1, "Id");
			const int id = static_cast<int>(luaL_optinteger(L, -1, 0));
			lua_pop(L, 1);
			const char* key = luaL_checkstring(L, 2);
			if (key && std::strcmp(key, "Checked") == 0)
			{
				std::lock_guard<std::mutex> lock(gMenuMu);
				for (const auto& it : gItems)
				{
					if (it.id == id)
					{
						lua_pushboolean(L, it.checked ? 1 : 0);
						return 1;
					}
				}
			}
			if (key && std::strcmp(key, "Add") == 0)
			{
				lua_pushcfunction(L, Menu_Add);
				return 1;
			}
			if (key && std::strcmp(key, "Remove") == 0)
			{
				lua_pushcfunction(L, Menu_Remove);
				return 1;
			}
			lua_getfield(L, 1, key); /* fallthrough raw — may be nil */
			return 1;
		}

		int Menu_Add(lua_State* L)
		{
			int parentId = 0;
			int arg = 1;
			if (lua_istable(L, 1))
			{
				lua_getfield(L, 1, "Id");
				if (lua_isinteger(L, -1))
					parentId = static_cast<int>(lua_tointeger(L, -1));
				lua_pop(L, 1);
				if (lua_type(L, 2) == LUA_TSTRING)
					arg = 2;
			}
			const char* name = luaL_checkstring(L, arg);
			/* Blish allows nil OnClick (labels / section headers). */
			int onClickRef = LUA_NOREF;
			if (lua_isfunction(L, arg + 1))
			{
				lua_pushvalue(L, arg + 1);
				onClickRef = luaL_ref(L, LUA_REGISTRYINDEX);
			}
			const bool canCheck = lua_toboolean(L, arg + 2) != 0;
			const bool checked = lua_toboolean(L, arg + 3) != 0;
			const char* tip = luaL_optstring(L, arg + 4, "");

			/* Reuse existing sibling with same name under same parent. */
			{
				std::lock_guard<std::mutex> lock(gMenuMu);
				for (auto& it : gItems)
				{
					if (it.parentId == parentId && it.name == (name ? name : ""))
					{
						PushMenuRef(L, it.id);
						return 1;
					}
				}
			}

			MenuItem item;
			item.id = gNextId++;
			item.parentId = parentId;
			item.name = name ? name : "";
			item.canCheck = canCheck;
			item.checked = checked;
			item.tooltip = tip ? tip : "";
			item.onClickRef = onClickRef;
			const int id = item.id;

			{
				std::lock_guard<std::mutex> lock(gMenuMu);
				gMenuL = L;
				gItems.push_back(std::move(item));
			}

			PushMenuRef(L, id);
			return 1;
		}

		int Menu_Remove(lua_State* L)
		{
			int arg = 1;
			if (lua_istable(L, 1) && lua_gettop(L) >= 2)
				arg = 2;
			int id = 0;
			std::string name;
			if (lua_istable(L, arg))
			{
				lua_getfield(L, arg, "Id");
				if (lua_isinteger(L, -1))
					id = static_cast<int>(lua_tointeger(L, -1));
				lua_pop(L, 1);
				lua_getfield(L, arg, "Name");
				if (lua_isstring(L, -1))
					name = lua_tostring(L, -1);
				lua_pop(L, 1);
			}
			else if (lua_isstring(L, arg))
				name = lua_tostring(L, arg);
			else
				id = static_cast<int>(luaL_optinteger(L, arg, 0));

			std::lock_guard<std::mutex> lock(gMenuMu);
			for (auto it = gItems.begin(); it != gItems.end(); ++it)
			{
				if ((id && it->id == id) ||
					(!name.empty() && it->name == name))
				{
					UnrefItem(L, *it);
					gItems.erase(it);
					break;
				}
			}
			return 0;
		}

		void DrawBranch(int parentId)
		{
			for (auto& it : gItems)
			{
				if (it.parentId != parentId)
					continue;
				ImGui::PushID(it.id);
				bool hasChild = false;
				for (const auto& c : gItems)
				{
					if (c.parentId == it.id)
					{
						hasChild = true;
						break;
					}
				}
				if (hasChild)
				{
					if (ImGui::TreeNode(it.name.c_str()))
					{
						DrawBranch(it.id);
						ImGui::TreePop();
					}
				}
				else if (it.canCheck)
				{
					if (ImGui::Checkbox(it.name.c_str(), &it.checked))
						FireClick(gMenuL, it);
				}
				else if (ImGui::Button(it.name.c_str()))
					FireClick(gMenuL, it);
				if (!it.tooltip.empty() && ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", it.tooltip.c_str());
				ImGui::PopID();
			}
		}
	}

	void PushMenuRef(lua_State* L, int id)
	{
		std::string name;
		{
			std::lock_guard<std::mutex> lock(gMenuMu);
			for (const auto& it : gItems)
			{
				if (it.id == id)
				{
					name = it.name;
					break;
				}
			}
		}
		lua_newtable(L);
		lua_pushinteger(L, id);
		lua_setfield(L, -2, "Id");
		lua_pushstring(L, name.c_str());
		lua_setfield(L, -2, "Name");
		lua_pushcfunction(L, Menu_Add);
		lua_setfield(L, -2, "Add");
		lua_pushcfunction(L, Menu_Remove);
		lua_setfield(L, -2, "Remove");
		lua_newtable(L);
		lua_pushcfunction(L, Menu_GetChecked);
		lua_setfield(L, -2, "__index");
		lua_pushcfunction(L, Menu_SetChecked);
		lua_setfield(L, -2, "__newindex");
		lua_setmetatable(L, -2);
	}

	void ClearMenus(lua_State* L)
	{
		std::lock_guard<std::mutex> lock(gMenuMu);
		for (auto& it : gItems)
			UnrefItem(L ? L : gMenuL, it);
		gItems.clear();
		gMenuL = nullptr;
	}

	void DrawMenus()
	{
		std::lock_guard<std::mutex> lock(gMenuMu);
		if (gItems.empty())
			return;
		PadNav::PushWrap();
		ImGui::Spacing();
		ImGui::TextColored(HelperTheme::Muted, "Script menus");
		DrawBranch(0);
		PadNav::PopWrap();
	}

	void RegisterMenu(lua_State* L)
	{
		/* Root menu object — Blish: Menu = new("Scripts", null, ...) */
		lua_newtable(L);
		lua_pushinteger(L, 0);
		lua_setfield(L, -2, "Id");
		lua_pushstring(L, "Scripts");
		lua_setfield(L, -2, "Name");
		lua_pushcfunction(L, Menu_Add);
		lua_setfield(L, -2, "Add");
		lua_pushcfunction(L, Menu_Remove);
		lua_setfield(L, -2, "Remove");
		lua_setglobal(L, "Menu");
	}
}

void PathingLua::DrawScriptMenus()
{
	if (!Enabled())
		return;
	PathingLuaDetail::DrawMenus();
}
