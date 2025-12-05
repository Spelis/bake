#include "util.h"

#include <lua.h>
#include <lua5.3/lauxlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int exists(const char* fname) {
	FILE* file;
	if ((file = fopen(fname, "r"))) {
		fclose(file);
		return 1;
	}
	return 0;
}

char* read_file(const char* filename) {
	FILE* f = fopen(filename, "rb");
	if (!f) {
		perror("fopen");
		return NULL;
	}

	// move to end to get file size
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	rewind(f);

	// allocate buffer (+1 for null terminator)
	char* buffer = malloc(size + 1);
	if (!buffer) {
		perror("malloc");
		fclose(f);
		return NULL;
	}

	// read file into buffer
	size_t read_bytes = fread(buffer, 1, size, f);
	if (read_bytes != size) {
		perror("fread");
		free(buffer);
		fclose(f);
		return NULL;
	}
	buffer[size] = '\0';  // null terminate

	fclose(f);
	return buffer;
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
	for (int i = 0; i < recipe_arr.count; i++) {
		Recipe* r = &recipe_arr.data[i];

		// Free Lua function reference if any
		if (r->function != LUA_NOREF) {
			luaL_unref(L, LUA_REGISTRYINDEX, r->function);
			r->function = LUA_NOREF;
		}
	}

	// Free the array itself
	free(recipe_arr.data);
	recipe_arr.data = NULL;
	recipe_arr.count = 0;
	recipe_arr.capacity = 0;
}
