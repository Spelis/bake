
#include <lua5.3/lauxlib.h>
#include <lua5.3/lua.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "bake.h"
int l_recipe(lua_State* L) {
	if (!lua_isstring(L, 1))
		return luaL_error(L, "Expected string as first argument");
	if (!lua_istable(L, 2))
		return luaL_error(L, "Expected table as second argument");
	if (!lua_isfunction(L, 3))
		return luaL_error(L, "Expected function as third argument");

	const char* luaTarget = lua_tostring(L, 1);
	size_t tableLen = lua_rawlen(L, 2);

	// Allocate dependency array
	char** depTable = malloc(tableLen * sizeof(char*));
	if (!depTable) return luaL_error(L, "Memory allocation failed");

	bool wildcard = strchr(luaTarget, '%') != NULL;

	for (size_t i = 0; i < tableLen; i++) {
		depTable[i] = NULL;	 // initialize
		lua_rawgeti(L, 2, i + 1);
		if (lua_isstring(L, -1)) {
			const char* s = lua_tostring(L, -1);
			depTable[i] = strdup(s);
			if (!depTable[i]) {
				// Cleanup on allocation failure
				for (size_t j = 0; j < i; j++) free(depTable[j]);
				free(depTable);
				lua_pop(L, 1);
				return luaL_error(L, "Memory allocation failed for dependency");
			}
			if (strchr(s, '%')) wildcard = true;
		}
		lua_pop(L, 1);
	}

	Recipe newRecipe = {0};
	int luaFuncRef = luaL_ref(L, LUA_REGISTRYINDEX);  // store Lua function

	if (wildcard) {
		newRecipe.target = NULL;
		newRecipe.pattern_target = strdup(luaTarget);
		if (!newRecipe.pattern_target) {
			for (size_t i = 0; i < tableLen; i++) free(depTable[i]);
			free(depTable);
			return luaL_error(L, "Memory allocation failed for pattern_target");
		}
		newRecipe.dependencies = depTable;
		newRecipe.pattern_deps =
			depTable;  // safe as long as you never free independently
		newRecipe.deplen = (int)tableLen;
		newRecipe.function = luaFuncRef;
		newRecipe.is_wildcard = true;
	} else {
		newRecipe.target = strdup(luaTarget);
		if (!newRecipe.target) {
			for (size_t i = 0; i < tableLen; i++) free(depTable[i]);
			free(depTable);
			return luaL_error(L, "Memory allocation failed for target");
		}
		newRecipe.dependencies = depTable;
		newRecipe.pattern_target = NULL;
		newRecipe.pattern_deps = NULL;
		newRecipe.deplen = (int)tableLen;
		newRecipe.function = luaFuncRef;
		newRecipe.is_wildcard = false;
	}

	recipe_add(newRecipe);
	return 0;
}
