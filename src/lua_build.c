#include <lua5.3/lauxlib.h>
#include <lua5.3/lua.h>

#include "bake.h"

int l_bake(lua_State* L) {
	if (!lua_istable(L, 1)) {
		return luaL_error(L, "Expected table as argument.");
	}
	expand_wildcard_recipes(L);

	lua_pushnil(L);
	LOG("\x1b[33mBaking...\x1b[0m");
	if (args.target_count == 0 || args.keep_defaults == 1) {
		while (lua_next(L, 1) != 0) {
			if (lua_type(L, -1) == LUA_TSTRING) {
				indent_log(1);
				const char* value = lua_tostring(L, -1);
				build(L, value);
				indent_log(-1);
			}
			lua_pop(L, 1);
		}
	}
	// forgive me for this warcrime.
	if (args.target_count >= 1) {
		for (int i = 0; i < args.target_count; i++) {
			indent_log(1);
			build(L, args.targets[i]);
			indent_log(-1);
		}
	}

	build_cleanup();

	if (successful == 0) {
		LOG("\x1b[33mThe cake is ready.\x1b[0m");
	} else if (successful == 1) {
		LOG("\x1b[33mThe cake might be malformed.\x1b[0m");
	} else {
		LOG("\x1b[33mThe cake is burnt.\x1b[0m");
	}
	return 0;
}
