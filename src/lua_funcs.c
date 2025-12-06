#include "lua_funcs.h"

#include <dirent.h>
#include <errno.h>
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

		// Special dependency that always forces rebuild
		if (strcmp(dep, "ALWAYS") == 0) {
			return true;
		}

		struct stat st_dep;
		if (stat(dep, &st_dep) != 0) {
			// Missing dependency -> assume target is out-of-date
			return true;
		}

		// Skip directories
		if (S_ISDIR(st_dep.st_mode)) {
			continue;
		}

		// If target does not exist or dependency is newer
		if (!target_exists || st_dep.st_mtime > target_mtime) {
			return true;
		}
	}

	// All dependencies older than target -> up-to-date
	return false;
}

void expand_wildcard_recipes(lua_State* L) {
	const char* target_suffix = ".o";

	for (int i = 0; i < recipe_arr.count; i++) {
		Recipe* wildcard_recipe = &recipe_arr.data[i];
		if (!wildcard_recipe->is_wildcard) continue;

		const char* pattern_target =
			wildcard_recipe->pattern_target;  // e.g., build/%.o

		for (int d = 0; d < wildcard_recipe->deplen; d++) {
			const char* pattern_dep =
				wildcard_recipe->pattern_deps[d];  // e.g., src/%.c

			char* dep_copy = strdup(pattern_dep);
			if (!dep_copy) continue;

			const char* dir = dirname(dep_copy);  // scan this dir
			DIR* dir_stream = opendir(dir);
			free(dep_copy);
			if (!dir_stream) continue;

			struct dirent* entry;
			size_t dep_suffix_len = strlen(
				strrchr(pattern_dep, '.') ? strrchr(pattern_dep, '.') : "");

			while ((entry = readdir(dir_stream)) != NULL) {
				if (entry->d_type != DT_REG) continue;

				const char* filename = entry->d_name;
				size_t name_len = strlen(filename);
				if (name_len <= dep_suffix_len) continue;  // too short

				// check if filename ends with same suffix as pattern_dep
				if (dep_suffix_len > 0 &&
					strcmp(filename + name_len - dep_suffix_len,
						   pattern_dep + strlen(pattern_dep) -
							   dep_suffix_len) != 0)
					continue;

				// compute stem dynamically
				size_t stem_len = name_len - dep_suffix_len;
				char* stem = malloc(stem_len + 1);
				if (!stem) continue;
				strncpy(stem, filename, stem_len);
				stem[stem_len] = '\0';

				// build concrete target and dependency paths
				size_t tgt_len =
					strlen("build/") + stem_len + strlen(target_suffix) + 1;
				char* concrete_target = malloc(tgt_len);
				if (!concrete_target) {
					free(stem);
					continue;
				}
				snprintf(concrete_target, tgt_len, "build/%s%s", stem,
						 target_suffix);

				size_t dep_len = strlen("src/") + stem_len + dep_suffix_len + 1;
				char* concrete_dep = malloc(dep_len);
				if (!concrete_dep) {
					free(stem);
					free(concrete_target);
					continue;
				}
				const char* dep_suffix =
					pattern_dep + strlen(pattern_dep) - dep_suffix_len;
				snprintf(concrete_dep, dep_len, "src/%s%s", stem, dep_suffix);

				// create new recipe
				Recipe new_recipe;
				new_recipe.target = concrete_target;
				new_recipe.dependencies = malloc(sizeof(char*));
				new_recipe.dependencies[0] = concrete_dep;
				new_recipe.deplen = 1;
				new_recipe.function = wildcard_recipe->function;
				new_recipe.is_wildcard = false;
				new_recipe.pattern_target = NULL;
				new_recipe.pattern_deps = NULL;

				recipe_add(new_recipe);
				free(stem);
			}
			closedir(dir_stream);
		}

		wildcard_recipe->is_wildcard = false;  // mark as expanded
	}
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

void build(lua_State* L, const char* target) {
	if (!target || target[0] == '\0') {
		LOG("ERR: Empty string passed to build");
		return;
	}

	Recipe* recipe = recipe_find((char*)target);
	if (!recipe) return;

	// Prevent cycles: check visited first
	for (int i = 0; i < visited.count; i++) {
		if (strcmp(visited.data[i]->target, target) == 0) return;
	}

	// Recursively build dependencies first
	for (int i = 0; i < recipe->deplen; i++) {
		const char* dep = recipe->dependencies[i];
		if (strcmp(dep, "ALWAYS") != 0) {
			build(L, dep);
		}
	}

	if (!args.force &&
		!is_out_of_date(recipe->target, recipe->dependencies, recipe->deplen)) {
		LOG("\x1b[35m\"%s\"\x1b[32m is fresh, serving...\x1b[0m", target);
		return;
	}

	// Add recipe to visited safely
	if (visited.count >= visited.capacity) {
		size_t new_cap = visited.capacity ? visited.capacity * 2 : 8;
		Recipe** tmp = realloc(visited.data, new_cap * sizeof(*visited.data));
		if (!tmp) {
			perror("realloc");
			build_cleanup();
			exit(EXIT_FAILURE);
		}
		visited.data = tmp;
		visited.capacity = new_cap;
	}

	Recipe* copy = malloc(sizeof(Recipe));
	if (!copy) {
		perror("malloc");
		build_cleanup();
		exit(EXIT_FAILURE);
	}
	*copy = *recipe;
	copy->target = strdup(recipe->target);
	if (!copy->target) {
		perror("strdup");
		free(copy);
		build_cleanup();
		exit(EXIT_FAILURE);
	}
	visited.data[visited.count++] = copy;

	// Push Lua function and arguments
	lua_rawgeti(L, LUA_REGISTRYINDEX, recipe->function);
	lua_pushstring(L, target);

	lua_newtable(L);
	for (int i = 0; i < recipe->deplen; i++) {
		lua_pushstring(L, recipe->dependencies[i]);
		lua_rawseti(L, -2, i + 1);
	}

	LOG("\x1b[34mBaking recipe \x1b[35m\"%s\"\x1b[0m", target);
	indent_log(1);
	if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
		const char* err = lua_tostring(L, -1);
		LOG("Error calling function: %s", err);
		indent_log(-1);
		lua_pop(L, 1);
		build_cleanup();
		exit(EXIT_FAILURE);
	}
	indent_log(-1);
	lua_pop(L, 1);
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

int l_yell(lua_State* L) {
	const char* msg = luaL_checkstring(L, 1);
	LOG("%s", msg);
	return 0;
}

int l_whisk(lua_State* L) {
	const char* cmd = luaL_checkstring(L, 1);  // safe check

	LOG("\x1b[2;90m$ %s\x1b[0m", cmd);

	FILE* pipe = popen(cmd, "r");
	if (!pipe) return luaL_error(L, "Failed to run command");

	// Read command output dynamically
	size_t bufsize = 1024;
	size_t len = 0;
	char* output = malloc(bufsize);
	if (!output) {
		pclose(pipe);
		return luaL_error(L, "Memory allocation failed");
	}

	int c;
	while ((c = fgetc(pipe)) != EOF) {
		if (len + 1 >= bufsize) {
			bufsize *= 2;
			char* tmp = realloc(output, bufsize);
			if (!tmp) {
				free(output);
				pclose(pipe);
				return luaL_error(L, "Memory allocation failed");
			}
			output = tmp;
		}
		output[len++] = (char)c;
	}
	output[len] = '\0';

	int ret = pclose(pipe);
	if (ret == -1) {
		free(output);
		return luaL_error(L, "Failed to close command pipe");
	}

	// Push result table
	lua_newtable(L);
	lua_pushinteger(L, WEXITSTATUS(ret));
	lua_setfield(L, -2, "return_code");
	lua_pushstring(L, output);
	lua_setfield(L, -2, "output");

	free(output);
	return 1;
}

int l_buy(lua_State* L) {
	const char* path = lua_tostring(L, 1);
	if (!path) return luaL_error(L, "Expected path string");
	FILE* f = fopen(path, "a");
	if (!f) return luaL_error(L, "Cannot create file %s", path);
	fclose(f);
	LOG("\x1b[2;90mCreated %s\x1b[0m", path);
	return 0;
}

int l_trash(lua_State* L) {
	const char* path = luaL_checkstring(L, 1);
	if (unlink(path) != 0) {
		return luaL_error(L, "Failed to remove '%s': %s", path,
						  strerror(errno));
	}
	LOG("\x1b[2;90mDeleted %s\x1b[0m", path);
	return 0;
}

int l_create_shelf(lua_State* L) {
	const char* path = luaL_checkstring(L, 1);

	size_t len = strlen(path);
	char* tmp = malloc(len + 2);  // +1 for null, +1 extra
	if (!tmp) return luaL_error(L, "Memory allocation failed");

	strcpy(tmp, path);

	for (char* p = tmp + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
				free(tmp);
				return luaL_error(L, "mkdir failed for %s: %s", tmp,
								  strerror(errno));
			}
			*p = '/';
		}
	}

	if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
		free(tmp);
		return luaL_error(L, "mkdir failed for %s: %s", tmp, strerror(errno));
	}

	free(tmp);
	LOG("\x1b[2;90mCreated %s/\x1b[0m", path);
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
	const char* dir = luaL_optstring(L, 1, ".");

	DIR* d = opendir(dir);
	if (!d)
		return luaL_error(L, "Cannot open directory '%s': %s", dir,
						  strerror(errno));

	lua_newtable(L);
	int i = 1;
	struct dirent* entry;
	while ((entry = readdir(d)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		// Allocate and copy name dynamically
		size_t len = strlen(entry->d_name) + 1;
		char* name = malloc(len);
		if (!name) {
			closedir(d);
			return luaL_error(L, "Memory allocation failed");
		}
		strcpy(name, entry->d_name);

		lua_pushstring(L, name);
		lua_rawseti(L, -2, i++);
		free(name);
	}

	closedir(d);
	return 1;
}

// Recursive helper
static void collect_impl(lua_State* L, const char* dir, const char* pattern,
						 int* index) {
	DIR* d = opendir(dir);
	if (!d) {
		luaL_error(L, "Cannot open directory '%s': %s", dir, strerror(errno));
		return;	 // won't reach
	}

	struct dirent* e;
	while ((e = readdir(d)) != NULL) {
		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;

		// Allocate full path dynamically
		size_t path_len = strlen(dir) + 1 + strlen(e->d_name) + 1;
		char* path = malloc(path_len);
		if (!path) {
			closedir(d);
			luaL_error(L, "Memory allocation failed");
			return;
		}
		snprintf(path, path_len, "%s/%s", dir, e->d_name);

		struct stat st;
		if (stat(path, &st) == 0) {
			if (S_ISDIR(st.st_mode)) {
				// Recurse into subdirectory
				collect_impl(L, path, pattern, index);
			} else if (!pattern || strstr(e->d_name, pattern)) {
				lua_pushstring(L, path);
				lua_rawseti(L, -2, (*index)++);
			}
		}
		free(path);
	}

	closedir(d);
}

int l_collect(lua_State* L) {
	const char* dir = luaL_checkstring(L, 1);
	const char* pattern = luaL_optstring(L, 2, NULL);

	lua_newtable(L);  // result table
	int index = 1;
	collect_impl(L, dir, pattern, &index);

	return 1;
}

// Transform one path dynamically
static char* transform_path(const char* in, const char* src_pre,
							const char* dst_pre, const char* old_ext,
							const char* new_ext) {
	const char* s = in;

	// Remove src prefix
	size_t src_len = strlen(src_pre);
	if (strncmp(in, src_pre, src_len) == 0) s += src_len;

	// Remove old extension if it matches
	const char* dot = strrchr(s, '.');
	size_t stem_len =
		dot && strcmp(dot, old_ext) == 0 ? (size_t)(dot - s) : strlen(s);

	size_t out_len = strlen(dst_pre) + stem_len + strlen(new_ext) + 1;
	char* out = malloc(out_len);
	if (!out) return NULL;

	snprintf(out, out_len, "%s%.*s%s", dst_pre, (int)stem_len, s, new_ext);
	return out;
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
		const char* p = luaL_checkstring(L, -1);

		char* out = transform_path(p, src_pre, dst_pre, old_ext, new_ext);
		if (!out) {
			lua_pop(L, 1);
			return luaL_error(L, "Memory allocation failed in transform_path");
		}

		lua_pop(L, 1);
		lua_pushstring(L, out);
		lua_rawseti(L, -2, i);
		free(out);
	}

	return 1;
}
