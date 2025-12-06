#pragma once
#include <lua5.3/lua.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

#include "argparse.h"
int exists(const char* fname);

typedef struct Recipe {
	char* target;
	char** dependencies;
	int deplen;
	int function;
	bool is_wildcard;
	char* pattern_target;
	char** pattern_deps;
} Recipe;

typedef struct {
	Recipe* data;
	size_t count;
	size_t capacity;
} RecipeArray;

typedef struct {
	Recipe** data;
	size_t count;
	size_t capacity;
} RecipeArrayP;

extern RecipeArray recipe_arr;

void recipe_add(Recipe recipe);
Recipe* recipe_find(char* target);
void recipes_free(lua_State* L);

extern BakeOptions args;
