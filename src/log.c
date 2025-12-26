#include <lua5.3/lauxlib.h>

#include "bake.h"

// i'll stick it in here just because, it has to do with logging
int l_yell(lua_State* L) {
	const char* msg = luaL_checkstring(L, 1);
	print("%s", msg);
	return 0;
}

static int indentation = 0;

int indent_log(int delta) {
	indentation += delta;
	if (indentation < 0) indentation = 0;
	return indentation;
}

void print(const char* fmt, ...) {
	int ind = indentation * 2;
	char buffer[2048];

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	const char* p = buffer;
	int at_start = 1;
	while (*p) {
		if (at_start) {
			for (int i = 0; i < ind; i++) fputc(' ', stderr);
			at_start = 0;
		}
		fputc(*p, stderr);
		if (*p == '\n') {
			at_start = 1;
		}
		p++;
	}
	fputc('\n', stderr);
}
