#include <lua.h>
#include <lua5.3/lauxlib.h>
#include <lua5.3/lua.h>
#include <lua5.3/lualib.h>
#include <lualib.h>

#include "argparse.h"
#include "log.h"
#include "lua_funcs.h"
#include "util.h"

static const luaL_Reg bake_lib[] = {{"bake", l_bake},	{"recipe", l_recipe},
									{"whisk", l_whisk}, {"yell", l_yell},
									{"print", l_yell},	{NULL, NULL}};

BakeOptions args;

int main(int argc, char* argv[]) {
	recipe_arr.data = NULL;
	recipe_arr.count = 0;
	recipe_arr.capacity = 0;

	lua_State* L = luaL_newstate();

	args = parse_args(L, argc, argv);

	if (!exists(args.file)) {
		LOG("\x1b[31mFile \"%s\" doesn't exist but is required for Bake to "
			"work.\x1b[0m",
			args.file);
		return 1;
	}

	luaopen_base(L);
	luaopen_string(L);
	luaopen_table(L);
	luaopen_math(L);
	luaL_openlibs(L);
	lua_pushnil(L);
	lua_setglobal(L, "print");
	for (const luaL_Reg* func = bake_lib; func->name != NULL; func++) {
		lua_pushcfunction(L, func->func);
		lua_setglobal(L, func->name);  // sets the function as a global
	}
	lua_newtable(L);  // create pantry table

	lua_pushcfunction(L, l_buy);
	lua_setfield(L, -2, "buy");	 // pantry.buy = l_buy

	lua_pushcfunction(L, l_trash);
	lua_setfield(L, -2, "trash");

	lua_pushcfunction(L, l_create_shelf);
	lua_setfield(L, -2, "new_shelf");

	lua_pushcfunction(L, l_is_shelf);
	lua_setfield(L, -2, "is_shelf");

	lua_pushcfunction(L, l_collect);
	lua_setfield(L, -2, "collect");

	lua_pushcfunction(L, l_objects);
	lua_setfield(L, -2, "objects");

	lua_newtable(L);  // metatable
	lua_pushcfunction(L, l_pantry);
	lua_setfield(L, -2, "__call");	// metatable.__call = l_pantry
	lua_setmetatable(L, -2);		// set metatable for pantry table

	lua_setglobal(L, "pantry");	 // set pantry table as global
	if (luaL_loadfile(L, "bake.lua") || lua_pcall(L, 0, 0, 0)) {
		LOG("Error: %s\n", lua_tostring(L, -1));
	}

	recipes_free(L);
	lua_close(L);
	return 0;
}
