// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "clif.hpp"
#include "lua_bridge.hpp"

namespace {
	struct script_context
	{
		map_session_data* sd;
		npc_data* nd;
		int args = 0;

		script_context(lua_State* L)
		{
			args = lua_gettop(L);
			sd = lua_helpers::extract_session_data(L);
			nd = lua_helpers::extract_npc_data(L);
		}

		bool is_valid()
		{
			return sd && nd;
		}
	};
}

namespace lua_bridge {

	int mes(lua_State* L)
	{
		script_context context(L);

		if (!context.is_valid()) {
			ShowWarning("[lua::mes]: Trying to call with no proper context.\n");
			return 0;
		}

		if (context.args < 1) {
			ShowWarning("[lua::mes]: Trying to call with no parameters.\n");
			return 0;
		}

		if (!lua_isstring(L, 1)) {
			ShowWarning("[lua::mes]: First parameter must be a string.\n");
			return 0;
		}

		const char* msg = lua_tostring(L, 1);
		clif_scriptmes(context.sd, context.nd->bl.id, msg);

		return 0;
	}

	int next(lua_State* L)
	{
		script_context context(L);

		if (!context.is_valid()) {
			return 0;
		}

		clif_scriptnext(context.sd, context.nd->bl.id);
		return lua_yield(L, 0);
	}

	int close(lua_State* L)
	{
		script_context context(L);

		if (!context.is_valid()) {
			return 0;
		}

		clif_scriptclose(context.sd, context.nd->bl.id);
		return 0;
	}
}

