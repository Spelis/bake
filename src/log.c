#include "log.h"

#include <lauxlib.h>
#include <lua5.3/lua.h>
#include <stdio.h>

static int g_indent = 0;

int indent_log(int delta) {
	g_indent += delta;
	if (g_indent < 0) g_indent = 0;
	return g_indent;
}

void LOG(const char* fmt, ...) {
	int ind = g_indent * LOG_INDENT;

	for (int i = 0; i < ind; i++) fputc(' ', stderr);

	/* message */
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fputc('\n', stderr);
}
