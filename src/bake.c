#include "bake.h"

#include <lua5.3/lauxlib.h>
#include <lua5.3/lua.h>
#include <lua5.3/lualib.h>
#include <sys/stat.h>

int exists(const char* fname) {
	struct stat st;
	return stat(fname, &st) == 0;
}

static const luaL_Reg bake_lib[] = {{"bake", l_bake},	{"recipe", l_recipe},
									{"whisk", l_whisk}, {"yell", l_yell},
									{"print", l_yell},	{NULL, NULL}};

BakeOptions args;

int main(int argc, char* argv[]) {
	args = parse_args(argc, argv);

	recipe_arr.data = NULL;
	recipe_arr.count = 0;
	recipe_arr.capacity = 0;

	if (!exists(args.file)) {
		print(
			"\x1b[31mFile \"%s\" doesn't exist but is required for Bake to "
			"work.\x1b[0m",
			args.file);
		return 1;
	}

	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	lua_pushnil(L);
	lua_setglobal(L, "print");
	for (const luaL_Reg* func = bake_lib; func->name != NULL; func++) {
		lua_pushcfunction(L, func->func);
		lua_setglobal(L, func->name);  // sets the function as a global
	}
	lua_newtable(L);  // create pantry table

	lua_pushcfunction(L, l_buy);
	lua_setfield(L, -2, "create");	// pantry.create = l_buy, etc..

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
	if (luaL_loadfile(L, args.file) || lua_pcall(L, 0, 0, 0)) {
		const char* err = lua_tostring(L, -1);
		print("\x1b[31mError loading/executing %s: %s\x1b[0m", args.file, err);
		lua_pop(L, 1);
		recipes_free(L);
		lua_close(L);
		return 1;
	}

	recipes_free(L);
	lua_close(L);
	return 0;
}
