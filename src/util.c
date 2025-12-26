#include <lua5.3/lauxlib.h>
#include <lua5.3/lua.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "bake.h"

int exists(const char* fname) {
	struct stat st;
	return stat(fname, &st) == 0;
}

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

		if (r->function != LUA_NOREF) {
			luaL_unref(L, LUA_REGISTRYINDEX, r->function);
			r->function = LUA_NOREF;
		}

		if (r->target) {
			free(r->target);
			r->target = NULL;
		}

		if (r->pattern_target) {
			free(r->pattern_target);
			r->pattern_target = NULL;
		}

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

		r->deplen = 0;
	}

	free(recipe_arr.data);
	recipe_arr.data = NULL;
	recipe_arr.count = 0;
	recipe_arr.capacity = 0;
}
