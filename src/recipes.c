#include <lua5.3/lauxlib.h>
#include <lua5.3/lua.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "bake.h"

RecipeArray recipe_arr;

void recipe_add(Recipe recipe) {
	if (recipe_arr.count >= recipe_arr.capacity) {
		if (recipe_arr.capacity == 0) recipe_arr.capacity = 8;
		recipe_arr.capacity *= 2;
		Recipe* tmp = realloc(recipe_arr.data,
							  recipe_arr.capacity * sizeof(*recipe_arr.data));
		if (!tmp) {
			perror("realloc");
			exit(EXIT_FAILURE);
		}
		recipe_arr.data = tmp;
	}
	recipe_arr.data[recipe_arr.count++] = recipe;
}

Recipe* recipe_find(char* target) {
	if (!target || !recipe_arr.data || recipe_arr.count == 0) return NULL;

	for (int i = 0; i < recipe_arr.count; i++) {
		if (!recipe_arr.data[i].target) continue;
		if (strcmp(target, recipe_arr.data[i].target) == 0) {
			return &recipe_arr.data[i];
		}
	}
	return NULL;
}

void recipes_free(lua_State* L) {
	if (!recipe_arr.data) return;

	for (size_t i = 0; i < recipe_arr.count; i++) {
		Recipe* r = &recipe_arr.data[i];

		// Free Lua function reference if any
		if (r->function != LUA_NOREF) {
			luaL_unref(L, LUA_REGISTRYINDEX, r->function);
			r->function = LUA_NOREF;
		}

		// Free target string
		if (r->target) {
			free(r->target);
			r->target = NULL;
		}

		// Free pattern target
		if (r->pattern_target) {
			free(r->pattern_target);
			r->pattern_target = NULL;
		}

		// Free dependencies
		if (r->dependencies) {
			for (int j = 0; j < r->deplen; j++) {
				if (r->dependencies[j]) {
					free(r->dependencies[j]);
					r->dependencies[j] = NULL;
				}
			}
			free(r->dependencies);
			r->dependencies = NULL;
		}

		// Don't free pattern dependencies

		r->deplen = 0;
	}

	// Free the array itself
	free(recipe_arr.data);
	recipe_arr.data = NULL;
	recipe_arr.count = 0;
	recipe_arr.capacity = 0;
}

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

	int wildcard = strchr(luaTarget, '%') != NULL;

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
			if (strchr(s, '%')) wildcard = 1;
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
		newRecipe.is_wildcard = 1;
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
		newRecipe.is_wildcard = 0;
	}

	recipe_add(newRecipe);
	return 0;
}
