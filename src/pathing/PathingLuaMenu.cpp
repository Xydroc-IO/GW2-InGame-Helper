#include "PathingLuaInternal.h"
#include "PathingLua.h"

#include "HelperTheme.h"
#include "PadNav.h"

#include "imgui/imgui.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

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
			lua_newtable(L);
			lua_pushinteger(L, it.id);
			lua_setfield(L, -2, "Id");
			lua_pushstring(L, it.name.c_str());
			lua_setfield(L, -2, "Name");
			lua_pushboolean(L, it.checked ? 1 : 0);
			lua_setfield(L, -2, "Checked");
			if (lua_pcall(L, 1, 0, 0) != LUA_OK)
			{
				LogLuaError(L, "Menu.Add callback");
				lua_pop(L, 1);
			}
		}

		int Menu_Add(lua_State* L)
		{
			int arg = 1;
			if (lua_istable(L, 1) && lua_type(L, 2) == LUA_TSTRING)
				arg = 2;
			const char* name = luaL_checkstring(L, arg);
			luaL_checktype(L, arg + 1, LUA_TFUNCTION);
			const bool canCheck = lua_toboolean(L, arg + 2) != 0;
			const bool checked = lua_toboolean(L, arg + 3) != 0;
			const char* tip = luaL_optstring(L, arg + 4, "");

			MenuItem item;
			item.id = gNextId++;
			item.name = name ? name : "";
			item.canCheck = canCheck;
			item.checked = checked;
			item.tooltip = tip ? tip : "";
			lua_pushvalue(L, arg + 1);
			item.onClickRef = luaL_ref(L, LUA_REGISTRYINDEX);
			const int id = item.id;

			{
				std::lock_guard<std::mutex> lock(gMenuMu);
				gMenuL = L;
				gItems.push_back(std::move(item));
			}

			lua_newtable(L);
			lua_pushinteger(L, id);
			lua_setfield(L, -2, "Id");
			lua_pushstring(L, name);
			lua_setfield(L, -2, "Name");
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
		for (auto& it : gItems)
		{
			ImGui::PushID(it.id);
			if (it.canCheck)
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
		PadNav::PopWrap();
	}

	void RegisterMenu(lua_State* L)
	{
		lua_newtable(L);
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
