#include "lua_funcs.h"

#include <dirent.h>
#include <libgen.h>
#include <lua5.3/lauxlib.h>
#include <lua5.3/lua.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "log.h"
#include "util.h"

static RecipeArrayP visited = {NULL, 0, 0};

bool is_out_of_date(const char* target, char** deps, int deplen) {
	struct stat st_target;
	bool target_exists = (stat(target, &st_target) == 0);

	time_t target_mtime = target_exists ? st_target.st_mtime : 0;

	for (int i = 0; i < deplen; i++) {
		const char* dep = deps[i];
		struct stat st_dep;

		if (strcmp(dep, "ALWAYS") == 0) {
			return true;
		}

		if (stat(dep, &st_dep) != 0) {
			// Dependency missing -> assume target is out-of-date
			return true;
		}

		if (S_ISDIR(st_dep.st_mode)) {
			continue;  // ignore directories for out-of-date check
		}

		if (!target_exists || st_dep.st_mtime > target_mtime) {
			return true;  // target older than dependency
		}
	}

	// All dependencies older than target -> up-to-date
	return false;
}

void expand_wildcard_recipes(lua_State* L) {
	for (int i = 0; i < recipe_arr.count; i++) {
		Recipe* r = &recipe_arr.data[i];
		if (!r->is_wildcard) continue;

		// assume single pattern for simplicity
		char* pat_tgt = r->pattern_target;	// e.g., build/%.o
		for (int d = 0; d < r->deplen; d++) {
			char* pat_dep = r->pattern_deps[d];			 // e.g., src/%.c
			const char* dir = dirname(strdup(pat_dep));	 // scan this dir
			DIR* dp = opendir(dir);
			if (!dp) continue;

			struct dirent* entry;
			while ((entry = readdir(dp)) != NULL) {
				if (entry->d_type != DT_REG) continue;

				const char* name = entry->d_name;
				// very simple matching: endswith .c
				size_t plen = strlen(pat_dep);
				size_t nlen = strlen(name);
				if (nlen + 1 < plen) continue;	// skip too short

				// build concrete target and deps
				char stem[256];
				strncpy(stem, name, nlen - 2);
				stem[nlen - 2] = '\0';

				char concrete_tgt[512];
				snprintf(concrete_tgt, sizeof(concrete_tgt), "build/%s.o",
						 stem);

				char concrete_dep[512];
				snprintf(concrete_dep, sizeof(concrete_dep), "src/%s.c", stem);

				Recipe newR;
				newR.target = strdup(concrete_tgt);
				newR.dependencies = malloc(sizeof(char*));
				newR.dependencies[0] = strdup(concrete_dep);
				newR.deplen = 1;
				newR.function = r->function;
				newR.is_wildcard = false;
				newR.pattern_target = NULL;
				newR.pattern_deps = NULL;
				recipe_add(newR);
			}
			closedir(dp);
		}

		r->is_wildcard = false;	 // mark as expanded
	}
}

void build(lua_State* L, const char* target) {
	if (!target || target[0] == '\0') {
		LOG("ERR: Empty string passed to build");
		return;
	}

	Recipe* foundRecipe = recipe_find((char*)target);
	if (!foundRecipe) return;  // recipe doesn't exist

	// recursively build dependencies first
	for (int i = 0; i < foundRecipe->deplen; i++) {
		if (strcmp(foundRecipe->dependencies[i], "ALWAYS") == 0) continue;
		build(L, foundRecipe->dependencies[i]);
	}

	if (!args.force) {
		if (!is_out_of_date(foundRecipe->target, foundRecipe->dependencies,
							foundRecipe->deplen)) {
			LOG("\x1b[35m\"%s\"\x1b[32m is fresh, serving...\x1b[0m", target);
			return;
		}
	}

	// skip if already visited
	for (int i = 0; i < visited.count; i++) {
		if (strcmp(visited.data[i]->target, target) == 0) return;
	}

	// resize visited array if needed
	if (visited.count >= visited.capacity) {
		visited.capacity = visited.capacity ? visited.capacity * 2 : 8;
		Recipe** tmp =
			realloc(visited.data, visited.capacity * sizeof(*visited.data));
		if (!tmp) {
			perror("realloc");
			exit(EXIT_FAILURE);
		}
		visited.data = tmp;
	}

	// copy the recipe and strdup target
	Recipe* copy = malloc(sizeof(Recipe));
	if (!copy) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	*copy = *foundRecipe;
	copy->target = strdup(foundRecipe->target);
	if (!copy->target) {
		perror("strdup");
		exit(EXIT_FAILURE);
	}
	visited.data[visited.count++] = copy;

	// push Lua function and arguments
	lua_rawgeti(L, LUA_REGISTRYINDEX, foundRecipe->function);
	lua_pushstring(L, target);

	lua_newtable(L);
	for (int i = 0; i < foundRecipe->deplen; i++) {
		lua_pushstring(L, foundRecipe->dependencies[i]);
		lua_rawseti(L, -2, i + 1);
	}

	LOG("\x1b[34mBaking recipe \x1b[35m\"%s\"\x1b[0m", target);
	indent_log(1);
	if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
		const char* err = lua_tostring(L, -1);
		LOG("Error calling function: %s", err);
		exit(EXIT_FAILURE);
	}
	indent_log(-1);
	lua_pop(L, 1);
}

void build_cleanup() {
	if (!visited.data) return;
	for (int i = 0; i < visited.count; i++) {
		free(visited.data[i]->target);
		free(visited.data[i]);
	}
	free(visited.data);
	visited.data = NULL;
	visited.count = visited.capacity = 0;
}

int l_bake(lua_State* L) {
	if (!lua_istable(L, 1)) {
		return luaL_error(L, "Expected table as argument.");
	}
	expand_wildcard_recipes(L);

	lua_pushnil(L);
	LOG("\x1b[33mBaking...\x1b[0m");
	if (args.target_count == 0) {
		while (lua_next(L, 1) != 0) {
			if (lua_type(L, -1) == LUA_TSTRING) {
				indent_log(1);
				const char* value = lua_tostring(L, -1);
				build(L, value);
				indent_log(-1);
			}
			lua_pop(L, 1);
		}
	} else {
		for (int i = 0; i < args.target_count; i++) {
			indent_log(1);
			build(L, args.targets[i]);
			indent_log(-1);
		}
	}

	build_cleanup();

	LOG("\x1b[33mThe cake is ready.\x1b[0m");
	return 0;
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

	char** depTable = malloc(tableLen * sizeof(char*));
	if (!depTable) return luaL_error(L, "Memory allocation failed");

	bool wildcard = strchr(luaTarget, '%') != NULL;

	for (size_t i = 0; i < tableLen; i++) {
		lua_rawgeti(L, 2, i + 1);
		if (lua_isstring(L, -1)) {
			const char* s = lua_tostring(L, -1);
			depTable[i] = strdup(s);
			if (!depTable[i]) depTable[i] = NULL;
			if (strchr(s, '%')) wildcard = true;
		} else {
			depTable[i] = NULL;
		}
		lua_pop(L, 1);
	}

	Recipe newRecipe;
	if (wildcard) {
		newRecipe.target = NULL;
		newRecipe.dependencies = depTable;
		newRecipe.deplen = (int)tableLen;
		newRecipe.function = luaL_ref(L, LUA_REGISTRYINDEX);
		newRecipe.is_wildcard = true;
		newRecipe.pattern_target = strdup(luaTarget);
		newRecipe.pattern_deps = depTable;
	} else {
		char* targetCopy = strdup(luaTarget);
		newRecipe.target = targetCopy;
		newRecipe.dependencies = depTable;
		newRecipe.deplen = (int)tableLen;
		newRecipe.function = luaL_ref(L, LUA_REGISTRYINDEX);
		newRecipe.is_wildcard = false;
		newRecipe.pattern_target = NULL;
		newRecipe.pattern_deps = NULL;
	}

	recipe_add(newRecipe);
	return 0;
}

int l_yell(lua_State* L) {
	LOG("%s", lua_tostring(L, 1));
	return 0;
}

int l_whisk(lua_State* L) {
	const char* cmd = lua_tostring(L, 1);
	if (!cmd) return luaL_error(L, "Expected string command");
	LOG("\x1b[2;90m$ %s\x1b[0m", cmd);
	int ret = system(cmd);
	lua_newtable(L);
	lua_pushinteger(L, ret);
	lua_setfield(L, -2, "return_code");
	lua_pushstring(L, "");	// no stdout captured here
	lua_setfield(L, -2, "output");
	return 1;
}

int l_buy(lua_State* L) {
	const char* path = lua_tostring(L, 1);
	if (!path) return luaL_error(L, "Expected path string");
	FILE* f = fopen(path, "a");
	if (!f) return luaL_error(L, "Cannot create file %s", path);
	fclose(f);
	return 0;
}

int l_trash(lua_State* L) {
	const char* path = lua_tostring(L, 1);
	if (!path) return luaL_error(L, "Expected path string");
	if (unlink(path) != 0) perror("unlink");
	return 0;
}

int l_create_shelf(lua_State* L) {
	const char* path = lua_tostring(L, 1);
	if (!path) return luaL_error(L, "Expected path string");
	char tmp[512];
	char* p;
	snprintf(tmp, sizeof(tmp), "%s", path);
	for (p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			mkdir(tmp, 0755);
			*p = '/';
		}
	}
	mkdir(tmp, 0755);
	return 0;
}

int l_is_shelf(lua_State* L) {
	const char* path = lua_tostring(L, 1);
	if (!path) {
		lua_pushboolean(L, 0);
		return 1;
	}
	struct stat st;
	if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
		lua_pushboolean(L, 1);
	} else {
		lua_pushboolean(L, 0);
	}
	return 1;
}

int l_pantry(lua_State* L) {
	const char* path = lua_tostring(L, 2);
	if (!path || path[0] == '\0') path = ".";

	DIR* d = opendir(path);
	if (!d) return luaL_error(L, "Cannot open directory %s", path);

	lua_newtable(L);
	int i = 1;
	struct dirent* entry;
	while ((entry = readdir(d)) != NULL) {
		if (strcmp(entry->d_name, "..") != 0 &&
			strcmp(entry->d_name, ".") != 0) {
			lua_pushstring(L, entry->d_name);
			lua_rawseti(L, -2, i++);
		}
	}
	closedir(d);
	return 1;
}

static void collect_impl(lua_State* L, const char* dir, const char* pattern,
						 int* index) {
	DIR* d = opendir(dir);
	if (!d) return;	 // or luaL_error if you want stricter rules

	struct dirent* e;
	char path[512];

	while ((e = readdir(d)) != NULL) {
		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;

		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);

		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			// recurse into subdirectory
			collect_impl(L, path, pattern, index);
		} else {
			// apply optional filter
			if (!pattern || strstr(e->d_name, pattern)) {
				lua_pushstring(L, path);
				lua_rawseti(L, -2, (*index)++);
			}
		}
	}

	closedir(d);
}

int l_collect(lua_State* L) {
	const char* dir = luaL_checkstring(L, 1);
	const char* pattern = luaL_optstring(L, 2, NULL);

	lua_newtable(L);  // result table
	int index = 1;	  // running index

	collect_impl(L, dir, pattern, &index);

	return 1;
}

static void transform_path(char* out, size_t outsz, const char* in,
						   const char* src_pre, const char* dst_pre,
						   const char* old_ext, const char* new_ext) {
	const char* s = in;

	// remove prefix
	if (strncmp(in, src_pre, strlen(src_pre)) == 0) s = in + strlen(src_pre);

	// remove + replace extension
	const char* dot = strrchr(s, '.');
	size_t stem_len =
		dot && strcmp(dot, old_ext) == 0 ? (size_t)(dot - s) : strlen(s);

	snprintf(out, outsz, "%s%.*s%s", dst_pre, (int)stem_len, s, new_ext);
}

int l_objects(lua_State* L) {
	luaL_checktype(L, 1, LUA_TTABLE);
	const char* src_pre = luaL_checkstring(L, 2);
	const char* dst_pre = luaL_checkstring(L, 3);
	const char* old_ext = luaL_checkstring(L, 4);
	const char* new_ext = luaL_checkstring(L, 5);

	size_t len = lua_rawlen(L, 1);
	lua_newtable(L);

	for (size_t i = 1; i <= len; i++) {
		lua_rawgeti(L, 1, i);
		const char* p = lua_tostring(L, -1);

		char out[512];
		transform_path(out, sizeof(out), p, src_pre, dst_pre, old_ext, new_ext);

		lua_pop(L, 1);
		lua_pushstring(L, out);
		lua_rawseti(L, -2, i);
	}

	return 1;
}
