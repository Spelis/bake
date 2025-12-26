#include <lua5.3/lauxlib.h>
#include <lua5.3/lua.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "bake.h"

int lw_handle_error(lua_State* L) {
	// upvalue 1 = result table
	luaL_checktype(L, lua_upvalueindex(1), LUA_TTABLE);

	// argument 1 = boolean flag
	luaL_checktype(L, 1, LUA_TBOOLEAN);
	int do_exit = lua_toboolean(L, 1);

	// fetch return_code
	lua_getfield(L, lua_upvalueindex(1), "return_code");
	int rc = luaL_optinteger(L, -1, 0);
	lua_pop(L, 1);

	if (rc == 0) {
		successful = 0;	 // all good
		return 0;
	}

	if (do_exit) {
		successful = 2;	 // catastrophic failure
		return luaL_error(L, "fatal error (return_code=%d)", rc);
	}

	successful = 1;	 // warning, continue
	return 0;
}

int l_whisk(lua_State* L) {
	const char* cmd = luaL_checkstring(L, 1);  // safe check

	LOG("\x1b[2;90m$ %s\x1b[0m", cmd);

	FILE* pipe = popen(cmd, "r");
	if (!pipe) return luaL_error(L, "Failed to run command");

	// Read command output dynamically
	size_t bufsize = 1024;
	size_t len = 0;
	char* output = malloc(bufsize);
	if (!output) {
		pclose(pipe);
		return luaL_error(L, "Memory allocation failed");
	}

	int c;
	while ((c = fgetc(pipe)) != EOF) {
		if (len + 1 >= bufsize) {
			bufsize *= 2;
			char* tmp = realloc(output, bufsize);
			if (!tmp) {
				free(output);
				pclose(pipe);
				return luaL_error(L, "Memory allocation failed");
			}
			output = tmp;
		}
		output[len++] = (char)c;
	}
	output[len] = '\0';

	int ret = pclose(pipe);
	if (ret == -1) {
		free(output);
		return luaL_error(L, "Failed to close command pipe");
	}

	// Push result table
	lua_newtable(L);
	lua_pushinteger(L, WEXITSTATUS(ret));
	lua_setfield(L, -2, "return_code");
	lua_pushstring(L, output);
	lua_setfield(L, -2, "output");
	lua_pushvalue(L, -1);
	lua_pushcclosure(L, lw_handle_error, 1);
	lua_setfield(L, -2, "err");

	free(output);
	return 1;
}
