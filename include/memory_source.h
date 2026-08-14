#pragma once

#include "qol.h"
#include <stdbool.h>

typedef struct memory_source memory_source_t;

typedef void *(*memory_source_alloc_fn)(void *ctx, usize size, usize alignment);

typedef void (*memory_source_free_fn)(void *ctx, void *ptr, usize size);

struct memory_source {
  memory_source_alloc_fn alloc;
  memory_source_free_fn free;
  void *ctx;
  bool returns_zeroed_memory;
};

/* Platform boundaries supplied with sigma. Custom sources can replace them
 * without changing allocator logic. */
extern const memory_source_t mmap_memory_source;
extern const memory_source_t malloc_memory_source;
