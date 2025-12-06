# Bake | The Bakery-themed Build System.

Bake is a lightweight build system written in C. It allows you to define recipes, wildcard rules, and dependencies, similar to Make, but with Lua scripting flexibility.

---

## Features

- Define recipes in Lua with dependencies.
- Support for wildcard patterns (`%.c`, `%.o`).
- Phony targets with the `"ALWAYS"` dependency.
- Automatic collection of source files and mapping to object files.
- Incremental builds: only rebuild targets when dependencies are out of date.
- Simple, color-coded logging.

---

## Installation

1. Clone the repository and install:

```bash
git clone https://github.com/spelis/bake.git
cd bake
make
```
> Note: Bake can build itself! Just run `bake` if you already have it.

2. Run Bake:

```bash
./build/bake
```

---

## Writing a Build Script (`bake.lua`)

Should be pretty easy to understand, read the `bake.lua` file for a base, here are some functions that Bake provides:

- `recipe(target,dependencies,callback)`: Defines a recipe
- `bake(default_targets)`: Executes all recipes inside the table, or from command line arguments
- `yell(str)`: Just an alias for `print`
- `whisk(cmd)`: Runs a shell command
- `pantry(dir?)`: List a directory
- `pantry.create(filename)`: Creates an empty file
- `pantry.trash(filename)`: Deletes a file
- `pantry.create_shelf(dirname)`: Creates a new directory
- `pantry.is_shelf(path)`: Checks if something is a directory
- `pantry.collect(dir, ext?)`: Collects all files (ending in `ext`, if you provide it) in a directory.
- `pantry.objects(src,dir,dst,src_ext,dst_ext)` I don't even want to explain this, example: `pantry.objects(pantry.collect("src",".c"), "src/", "build/", ".c", ".o")` changes `src/*.c` -> `build/*.o`

---

## License

MIT License: [LICENSE](LICENSE)
