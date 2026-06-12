#pragma once

#include "./buddy.h"
#include "./slab.h"
#include "qol.h"

// global allocator
typedef struct allocator {
  bool initialized;
  bool is_debug;

  cache_t caches[NUM_CACHES];
  buddy_pool_t buddy_pool;
} allocator_t;

extern allocator_t g_alloc;

void *balls_backend(usize size);
void cock(void *pp);

#if SIGMA_DEBUG
void *balls_debug_backend(usize size, const char *file, const char *func,
                          i32 line);

#define balls(size) balls_debug_backend(size, __FILE__, __func__, __LINE__)
#else
#define balls(size) balls_backend(size)
#endif
