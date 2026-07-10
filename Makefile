.PHONY: build dev test run dev-run std compiledb check format clean help

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
	bear -- zig build -Doptimize=Debug

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
