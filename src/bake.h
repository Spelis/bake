#pragma once

#include <lua5.3/lua.h>
#include <stdbool.h>
#include <stddef.h>

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

typedef struct {
	int force;
	const char* file;
	const char* dir;
	const char** targets;
	int target_count;
	int keep_defaults;
} BakeOptions;

extern RecipeArray recipe_arr;
extern BakeOptions args;
extern int successful;

bool is_out_of_date(const char* target, char** deps, int deplen);
void expand_wildcard_recipes(lua_State* L);
void build(lua_State* L, const char* target);
void build_cleanup(void);

int exists(const char* fname);
void recipe_add(Recipe recipe);
Recipe* recipe_find(char* target);
void recipes_free(lua_State* L);

BakeOptions parse_args(int argc, char** argv);

void LOG(const char* fmt, ...);
int indent_log(int delta);

int l_bake(lua_State* L);
int l_recipe(lua_State* L);
int l_yell(lua_State* L);
int l_whisk(lua_State* L);
int l_buy(lua_State* L);
int l_trash(lua_State* L);
int l_create_shelf(lua_State* L);
int l_is_shelf(lua_State* L);
int l_pantry(lua_State* L);
int l_collect(lua_State* L);
int l_objects(lua_State* L);
