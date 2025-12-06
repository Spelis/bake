#pragma once

#include <lua5.3/lua.h>

typedef struct {
	int force;
	const char* file;
	const char* dir;
	const char** targets;
	int target_count;
} BakeOptions;

BakeOptions parse_args(int argc, char** argv);
