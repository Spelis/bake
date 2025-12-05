# this Makefile does basically the same thing as the bake.lua file.

TARGET := $(shell basename $(shell pwd))

CCFLAGS := -g
LDFLAGS := -g -llua5.3

SRC_DIR := src
SRC     := $(shell find $(SRC_DIR) -name '*.c')
OBJ     := $(SRC:$(SRC_DIR)/%.c=build/%.o)

all: dirs build/$(TARGET)

dirs:
	@mkdir -p build/ $(sort $(dir $(OBJ)))

build/$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(CCFLAGS) $(LDFLAGS) -o $@

build/%.o: $(SRC_DIR)/%.c
	$(CC) $(CCFLAGS) -c $< -o $@

clean:
	rm -rf build
