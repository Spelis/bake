#include <stdlib.h>
#include <string.h>

#include "bake.h"

#ifndef VERSION
#define VERSION "Unknown"
#endif

#define HELP_STR                                                    \
	"Usage: bake [options] [rules]\n"                               \
	"\nArguments:\n"                                                \
	"  <rules>    Optional rule names to run instead of defaults\n" \
	"\nOptions:\n"                                                  \
	"  -B         Force all rules to be remade\n"                   \
	"  -f <file>  Specify a Bake Lua file (default: bake.lua)\n"    \
	"  -C <dir>   Use <dir> as the working directory\n"             \
	"  -d         Keeps defaults even with <rules> passed\n"        \
	"  -v         Print version information and exit\n"             \
	"  -h         Show this help message and exit\n"

BakeOptions parse_args(int argc, char** argv) {
	BakeOptions opts = {
		.force = 0,
		.file = "bake.lua",
		.dir = NULL,
		.targets = NULL,
		.target_count = 0,
		.keep_defaults = 0,
	};

	const char** targets = malloc(sizeof(char*) * argc);
	if (!targets) {
		print("Out of memory");
		exit(1);
	}

	int tcount = 0;

	for (int i = 1; i < argc; i++) {
		/* exit-immediately flags */
		if (strcmp(argv[i], "-h") == 0) {
			print(HELP_STR);
			exit(0);
		}

		if (strcmp(argv[i], "-v") == 0) {
			print("Bake version: %s", VERSION);
			exit(0);
		}

		/* flags with arguments */
		if (strcmp(argv[i], "-f") == 0) {
			if (++i >= argc) {
				print("Option -f requires a filename");
				exit(1);
			}
			opts.file = argv[i];
			continue;
		}

		if (strcmp(argv[i], "-C") == 0) {
			if (++i >= argc) {
				print("Option -C requires a directory");
				exit(1);
			}
			opts.dir = argv[i];
			continue;
		}

		/* simple flags */
		if (strcmp(argv[i], "-B") == 0) {
			opts.force = 1;
			continue;
		}

		if (strcmp(argv[i], "-d") == 0) {
			opts.keep_defaults = 1;
			continue;
		}

		/* positional arguments */
		if (argv[i][0] != '-') {
			targets[tcount++] = argv[i];
			continue;
		}

		/* unknown option */
		print("Unknown option: %s", argv[i]);
		exit(1);
	}

	opts.targets = targets;
	opts.target_count = tcount;
	return opts;
}
