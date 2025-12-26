#include <dirent.h>
#include <errno.h>
#include <lua5.3/lauxlib.h>
#include <lua5.3/lua.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bake.h"

int l_yell(lua_State* L) {
	const char* msg = luaL_checkstring(L, 1);
	LOG("%s", msg);
	return 0;
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
