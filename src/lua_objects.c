#include <lua5.3/lauxlib.h>
#include <stdlib.h>
#include <string.h>

#include "bake.h"

static char* transform_path(const char* in, const char* src_pre,
							const char* dst_pre, const char* old_ext,
							const char* new_ext) {
	const char* s = in;

	// Remove src prefix
	size_t src_len = strlen(src_pre);
	if (strncmp(in, src_pre, src_len) == 0) s += src_len;

	// Remove old extension if it matches
	const char* dot = strrchr(s, '.');
	size_t stem_len =
		dot && strcmp(dot, old_ext) == 0 ? (size_t)(dot - s) : strlen(s);

	size_t out_len = strlen(dst_pre) + stem_len + strlen(new_ext) + 1;
	char* out = malloc(out_len);
	if (!out) return NULL;

	snprintf(out, out_len, "%s%.*s%s", dst_pre, (int)stem_len, s, new_ext);
	return out;
}

int l_objects(lua_State* L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	const char* src_pre = luaL_checkstring(L, 2);
	const char* dst_pre = luaL_checkstring(L, 3);
	const char* old_ext = luaL_checkstring(L, 4);
	const char* new_ext = luaL_checkstring(L, 5);

	size_t len = lua_rawlen(L, 1);
	lua_newtable(L);

	for (size_t i = 1; i <= len; i++) {
		lua_rawgeti(L, 1, i);
		const char* p = luaL_checkstring(L, -1);

		char* out = transform_path(p, src_pre, dst_pre, old_ext, new_ext);
		if (!out) {
			lua_pop(L, 1);
			return luaL_error(L, "Memory allocation failed in transform_path");
		}

		lua_pop(L, 1);
		lua_pushstring(L, out);
		lua_rawseti(L, -2, i);
		free(out);
	}

	return 1;
}
