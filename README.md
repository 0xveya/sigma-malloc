# sigma_malloc

My fun side quest to learn more about memory allocation and C.

## About

Besides learning more about this topic, the goal is to have my own `malloc` implementation for post common-core projects in 42.

I want to create composable allocators on top of the generic core to have a Zig-like allocator experience, together with a debug mode to detect leaks. For example, I want to be able to create an arena allocator from the generic one, like in Zig, and pass the allocator into functions to handle lifetimes better.

I will also implement pluggable backends depending on the allowed functions. For example, one backend could use `malloc` instead of `mmap`.

## Current progress

- Basic memory allocation and freeing.
- Debug information for unfreed memory.

## Requirements

The project uses the Zig build system to build, test, and fuzz the code. The C sources currently target C23 and use Clang blocks (`-fblocks`). The expected Zig version is pinned in `mise.toml`:

- Zig 0.16.0.
- A C23 compiler/toolchain supported by Zig.
- `BlocksRuntime`.

`mise` can optionally install and manage the expected toolchain versions. If you already have Zig 0.16.0 and the required system libraries installed, you can use the `zig` and `make` commands directly.

## Building and running

Clone the repository and enter the project directory, then run a debug build:

```sh
mise install       # Optional: install the versions from mise.toml
zig build -Doptimize=Debug
./zig-out/bin/app-dev
```

To build and run the release version:

```sh
zig build -Doptimize=ReleaseFast
./zig-out/bin/app
```

The same common workflows are available through the `Makefile`:

```sh
make dev-run       # Build and run the debug version (the default)
make run           # Build and run the release version
make build         # Build the release version
make dev           # Build the debug version
```

## Testing

Run the regression tests and allocator fuzz tests with:

```sh
zig build test
```

The fuzz tests can also be run through the dedicated build step. Zig’s fuzzing options can be passed after `--` when needed:

```sh
zig build alloc-fuzz
zig build alloc-fuzz -- --fuzz
```

The stress-test executable can be run with:

```sh
zig build -Doptimize=Debug stress
make stress ARGS="--allocator system"
```

Useful maintenance commands are also available through `make`:

```sh
make check         # Run cppcheck
make format        # Format C and header files with clang-format
make compiledb     # Generate compile_commands.json
make clean         # Remove generated build artifacts
```

## Technical details

The allocator currently uses different strategies depending on the requested allocation size:

- Allocations below 1 KiB use a slab allocator.
- Allocations from 1 KiB up to 4 MiB use a buddy allocator.
- Allocations larger than 4 MiB use raw `mmap`.

It handles multithreading by using per-thread arenas, which keeps the common allocation path free of locks.

## TODO

- Make the backend generic and swappable.
- Make allocators composable like Zig.
- Have a 42 version that only uses my own functions instead of the standard library and `malloc`.
- Add a script to convert it into a single copy-pastable header and select which external functions are used. (make it also remove C23 specific thigns so i can compile on campus pc)
- Get rid of Clang blocks and make a fully standard C23 version( and a non.
