.PHONY: build dev test run dev-run std compiledb compiledb-commands check format clean help

MAKEFLAGS += -j

C_SOURCES := main.c $(wildcard src/*.c)
COMPILEDB_TARGETS := $(C_SOURCES:%=compiledb-%)
COMPILEDB_FLAGS := -std=c23 -fblocks -Wall -Wextra -Wpedantic -Wno-auto-decl-extensions -Wshadow -Wconversion -Wdouble-promotion -Wformat=2 -Wundef -I include -DHORNY_MODE=1 -DNO_LEAK_REWARD=1 -DUSE_DEBUG_ALLOC=1

.PHONY: $(COMPILEDB_TARGETS)

# Default target when you just type 'make'
all: dev-run

build:
	zig build -Doptimize=ReleaseFast

dev:
	zig build -Doptimize=Debug

test:
	zig build && [ -f zig-out/bin/tests ] && ./zig-out/bin/tests || echo 'No tests binary compiled (test.c not found)'

run: build
	./zig-out/bin/app

dev-run: dev
	./zig-out/bin/app-dev

std:
	zig std -p 8000 --no-open-browser

compiledb:
	compiledb --overwrite make compiledb-commands

compiledb-commands: $(COMPILEDB_TARGETS)

$(COMPILEDB_TARGETS):
	clang $(COMPILEDB_FLAGS) -fsyntax-only $(patsubst compiledb-%,%,$@)

check:
	cppcheck --enable=all --suppress=missingIncludeSystem main.c src/*.c

format:
	clang-format -i main.c src/*.c include/*.h

clean:
	rm -rf zig-out .zig-cache compile_commands.json .bear-fingerprints app app-dev tests

help:
	@echo "Available make targets:"
	@echo "  make build      - compile main (non debug)"
	@echo "  make dev        - compile main (debug/dev build)"
	@echo "  make test       - compile and run tests"
	@echo "  make run        - run release build"
	@echo "  make dev-run    - run debug/dev build (Default)"
	@echo "  make std        - run zig std documentation server"
	@echo "  make compiledb  - generate compile_commands.json for neovim"
	@echo "  make check      - run cppcheck static analysis"
	@echo "  make format     - format source files using clang-format"
	@echo "  make clean      - remove build artifacts"
