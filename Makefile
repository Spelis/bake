TARGET := $(shell basename $(shell pwd))

CCFLAGS := -g
LDFLAGS := -g -llua5.3

SRC_DIR := src
SRC     := $(shell find $(SRC_DIR) -name '*.c')
OBJ     := $(SRC:$(SRC_DIR)/%.c=build/%.o)
STUB    := bake_stubs.lua

all: dirs build/$(TARGET)

dirs:
	@mkdir -p build/ $(sort $(dir $(OBJ)))

build/$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(CCFLAGS) $(LDFLAGS) -o $@

build/%.o: $(SRC_DIR)/%.c
	$(CC) $(CCFLAGS) -D'VERSION="$(shell date +%y_%m_%d:%H.%M)"' -c $< -o $@

.PHONY: clean
clean:
	rm -rf build
