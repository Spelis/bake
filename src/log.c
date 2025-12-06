#include "log.h"

#include <stdio.h>

static int g_indent = 0;

int indent_log(int delta) {
	g_indent += delta;
	if (g_indent < 0) g_indent = 0;
	return g_indent;
}

void LOG(const char* fmt, ...) {
	int ind = g_indent * 2;
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
