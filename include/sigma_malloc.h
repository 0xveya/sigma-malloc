#pragma once

#include "buddy.h"
#include "debug.h"
#include "large.h"
#include "qol.h"
#include "slab.h"

// global allocator
typedef struct allocator {
  bool initialized;
  bool is_debug;

  cache_t caches[NUM_CACHES];
  buddy_pool_t buddy_pool;
  large_node_t *large_allocs_head;
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
