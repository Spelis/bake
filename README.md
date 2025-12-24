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
# or install it (bake exclusive)
./build/bake install
# now it's installed globally!
```

---

## License

MIT License: [LICENSE](LICENSE)
