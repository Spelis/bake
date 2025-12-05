#pragma once

#include <lua.h>

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
