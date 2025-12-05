#pragma once

#include <lua5.3/lua.h>
#include <stdarg.h>
#include <stdbool.h>

#ifndef LOG_INDENT
#define LOG_INDENT 2
#endif

typedef struct LogConfig {
	int stack_depth;	  // how many frames up the Lua stack to inspect
	const char* func;	  // override function name (or NULL for none)
	const char* file;	  // override file name (or NULL for none)
	bool want_lua_info;	  // inspect Lua stack?
	bool want_timestamp;  // print time?
} LogConfig;

void LOG(const char* fmt, ...);

int indent_log(int delta);
