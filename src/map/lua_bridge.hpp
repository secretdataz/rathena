// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef LUA_BRIDGE_HPP
#define LUA_BRIDGE_HPP

#include "../common/showmsg.hpp"

#include "npc.hpp"

#include <lua.hpp>


namespace lua_helpers {

	template<typename T>
	T* extract_user_data(lua_State* L, const char* name)
	{
		lua_getglobal(L, name);

		if (!lua_islightuserdata(L, -1)) {
			lua_pop(L, 1);
			return nullptr;
		}

		T* user_data = (T*)lua_touserdata(L, -1);

		lua_pop(L, 1);
		return user_data;
	}

	constexpr auto SD_LUA_VARIABLE = "__map_session_data__";
	constexpr auto ND_LUA_VARIABLE = "__npc_data__";

	inline map_session_data* extract_session_data(lua_State* L)
	{
		return extract_user_data<map_session_data>(L, SD_LUA_VARIABLE);
	}

	inline npc_data* extract_npc_data(lua_State* L)
	{
		return extract_user_data<npc_data>(L, ND_LUA_VARIABLE);
	}
}

/**
 * Group of functions that are meant to be used inside
 * lua scripts. They serve as a connection between the
 * server and the lua environment.
 */
namespace lua_bridge {
	int mes(lua_State*);
	int next(lua_State*);
	int close(lua_State*);
}

#endif
