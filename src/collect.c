#include <dirent.h>
#include <errno.h>
#include <lua5.3/lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "bake.h"

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
