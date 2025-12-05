#include "argparse.h"

#include <lua.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

BakeOptions parse_args(lua_State* L, int argc, char** argv) {
	BakeOptions opts = {0, "bake.lua", NULL, NULL, 0};
	const char** targets = malloc(sizeof(char*) * argc);
	int tcount = 0;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-B") == 0) {
			opts.force = 1;
		} else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
			opts.file = argv[++i];
		} else if (strcmp(argv[i], "-C") == 0 && i + 1 < argc) {
			opts.dir = argv[++i];
		} else if (argv[i][0] != '-') {
			targets[tcount++] = argv[i];
		} else {
			LOG("Unknown option: %s\n", argv[i]);
		}
	}

	opts.targets = targets;
	opts.target_count = tcount;
	return opts;
}
