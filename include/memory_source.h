#pragma once

#include "qol.h"

typedef struct memory_source memory_source_t;

typedef void *(*memory_source_alloc_fn)(void *ctx, usize size, usize alignment);

typedef void (*memory_source_free_fn)(void *ctx, void *ptr, usize size);

struct memory_source {
  memory_source_alloc_fn alloc;
  memory_source_free_fn free;
  void *ctx;
};

extern const memory_source_t mmap_memory_source;
extern const memory_source_t malloc_memory_source;
