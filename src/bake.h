#pragma once

#include <lua5.3/lua.h>

// Lua functions

int l_bake(lua_State* L);
int l_recipe(lua_State* L);
int l_whisk(lua_State* L);
int l_yell(lua_State* L);
int l_buy(lua_State* L);
int l_trash(lua_State* L);
int l_create_shelf(lua_State* L);
int l_pantry(lua_State* L);
int l_is_shelf(lua_State* L);
int l_collect(lua_State* L);
int l_objects(lua_State* L);

// Argument parsing

typedef struct {
	int force;
	const char* file;
	const char* dir;
	const char** targets;
	int target_count;
	int keep_defaults;
} BakeOptions;

BakeOptions parse_args(int argc, char** argv);
extern BakeOptions args;

// Logging

void print(const char* fmt, ...);

int indent_log(int delta);

// Recipes

typedef struct Recipe {
	char* target;
	char** dependencies;
	int deplen;
	int function;
	int is_wildcard;
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

// Utility functions

int exists(const char* fname);
