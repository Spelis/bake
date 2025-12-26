#include <dirent.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bake.h"

static RecipeArrayP visited = {0};

bool is_out_of_date(const char* target, char** deps, int deplen) {
	struct stat st_target;
	bool target_exists = (stat(target, &st_target) == 0);
	time_t target_mtime = target_exists ? st_target.st_mtime : 0;

	for (int i = 0; i < deplen; i++) {
		const char* dep = deps[i];

		if (strcmp(dep, "ALWAYS") == 0) {
			return true;
		}

		struct stat st_dep;
		if (stat(dep, &st_dep) != 0) {
			return true;
		}

		if (S_ISDIR(st_dep.st_mode)) {
			continue;
		}

		if (!target_exists || st_dep.st_mtime > target_mtime) {
			return true;
		}
	}

	return false;
}

void expand_wildcard_recipes(lua_State* L) {
	const char* target_suffix = ".o";

	for (int i = 0; i < recipe_arr.count; i++) {
		Recipe* wildcard_recipe = &recipe_arr.data[i];
		if (!wildcard_recipe->is_wildcard) continue;

		const char* pattern_target = wildcard_recipe->pattern_target;

		for (int d = 0; d < wildcard_recipe->deplen; d++) {
			const char* pattern_dep = wildcard_recipe->pattern_deps[d];

			char* dep_copy = strdup(pattern_dep);
			if (!dep_copy) continue;

			const char* dir = dirname(dep_copy);
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
				if (name_len <= dep_suffix_len) continue;

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

		wildcard_recipe->is_wildcard = false;
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
		LOG("\x1b[31mError calling function: %s\x1b[0m", err);
		if (successful == 2) {
			indent_log(-10);
			LOG("\x1b[33mThe cake has been cancelled.\x1b[0m");
		}
		indent_log(-1);
		lua_pop(L, 1);
		build_cleanup();
		exit(EXIT_FAILURE);
	}
	indent_log(-1);
	lua_pop(L, 1);
}
