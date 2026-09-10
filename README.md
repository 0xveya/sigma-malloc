# sigma_malloc

A small allocator project for learning how allocation works and building a
reusable allocator for post common-core 42 projects.

Sigma provides mmap- and malloc-backed allocator instances behind a generic
`allocator_t` interface. Allocators are composable: an arena can use any
generic allocator as its parent, then be passed into code without exposing its
allocation strategy.

## Features

- Instance-aware mmap and malloc memory sources.
- Slab, buddy, and direct backing-source allocation paths.
- Power-of-two alignment and typed allocation helpers.
- Realloc and zeroed allocation without direct allocator-runtime libc calls.
- A composable arena allocator.
- Leak source locations, wrong-owner errors, and double-free diagnostics in
  debug builds.

## Using the allocator

Typed helpers derive alignment from `_Alignof(T)`, so normal callers do not
need to pass alignment explicitly:

```c
#include "arena_allocator.h"
#include "memory_source.h"
#include "sigma_malloc.h"

sigma_allocator_t sigma;
sigma_allocator_init(&sigma, &mmap_memory_source);

allocator_t allocator = sigma_allocator(&sigma);
int *values = allocator_array_zeroed(allocator, int, 32);

if (values != NULL) {
  values[0] = 42;
  allocator_free_array(allocator, values, 32);
}

sigma_debug_report_leaks(&sigma);
```

Raw byte-oriented code can use `allocator_alloc_aligned`. Sigma records size,
alignment, ownership, and debug source information in its metadata. The
generic interface keeps size and alignment explicit so implementations that do
not store headers remain possible.

An arena composes over any generic allocator and releases all of its backing
blocks together:

```c
allocator_arena_t arena;
allocator_arena_init(&arena, allocator, 16 * 1024);

allocator_t temporary = allocator_arena(&arena);
int *scratch = allocator_array(temporary, int, 128);

if (scratch != NULL)
  scratch[0] = 42;

allocator_arena_deinit(&arena);
```

Individual arena frees are no-ops. Arena realloc preserves the old prefix, and
`allocator_arena_reset` or `allocator_arena_deinit` returns every backing block
to the parent.

The [`examples`](examples) directory contains a standalone parser whose state
stores an `allocator_t`. It parses a small configuration string, grows its
result array through the public API, and works with the arena supplied by its
caller:

```sh
xmake run parser-example
```

To intentionally leak the parser arena and inspect its allocation source:

```sh
xmake f --parser_example_free=n -y
xmake run parser-example
```

The debug report points to the user allocation through the composed arena:

```text
Memory Leak Detected:
  Location: parser.c:21 inside copy_slice()
     => char *copy = allocator_array(parser->allocator, char, length + 1);
```

## Build and test

The project uses C23, Clang, and Xmake, pinned in [`mise.toml`](mise.toml).
`mise` is optional when those tools are already installed.

```sh
mise install
mise run dev
mise run dev-run
mise run test
```

`mise run test` runs the regression and deterministic fuzz-corpus cases plus quiet
single-threaded, threaded, and arena stress validations. Successful cases are
shown as ANSI-colored checkmarks; stress output remains hidden unless a check
fails.

Other useful commands:

```sh
mise run build
mise run run
mise run stress
mise run check
mise run format
```

## Stress testing

The stress runner supports `custom`, `custom-arena`, and `system` allocators,
deterministic seeds, multiple workers, full or sampled verification, and human,
JSON, or quiet output. `--target` applies to each worker and is bounded by the
configured slot count and maximum allocation size.

```sh
xmake f -m release -y
xmake run allocator-stress -- \
  --allocator custom --threads 4 --target 256M --max-size 1M \
  --slots 8192 --cycles 20 --verify full --seed 12345
```

Use the same seed and workload to compare backends:

```sh
xmake run allocator-stress -- --allocator custom --target 256M \
  --seed 12345 --output json > custom-results.jsonl
xmake run allocator-stress -- --allocator system --target 256M \
  --seed 12345 --output json > system-results.jsonl
```

The slot table and allocator metadata add memory overhead beyond the live
payload. Choose targets that fit the machine running the test.

## Design

Allocation routing depends on size and alignment:

- Requests up to and including 1 KiB with supported alignment try the slab
  allocator.
- Larger requests that fit a buddy pool try the buddy allocator.
- Other requests use the allocator instance's selected memory source directly.

Internal arenas are per-thread and per-sigma-instance. They are separate from
the public arena allocator, whose lifetime is controlled by its caller.

Allocator runtime code reaches the host through
[`libc_wrappers.c`](src/libc_wrappers.c) and its matching header. That boundary
contains malloc/free, mmap/munmap, memset, and the optional libc memcpy call.
By default copying uses the local implementation; defining
`SIGMA_USE_LIBC_MEMCPY` selects libc memcpy. This keeps later 42-specific
routing changes in one place.

## TODO

- Provide a 42-compatible host-wrapper implementation and allowed-functions
  profile.
- Generate a single-header distribution that can also target pre-C23 campus
  toolchains.
