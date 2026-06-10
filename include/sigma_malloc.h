#pragma once

#include "./slab.h"

// global allocator
typedef struct allocator {
  bool initialized;
  bool is_debug;

  cache_t caches[NUM_CACHES];
} allocator_t;

extern allocator_t g_alloc;

void *balls_backend(size_t size);
void cock(void *pp);

#define balls(size)                                                            \
  __extension__({                                                              \
    size_t __sz = (size);                                                      \
    void *__ptr = balls_backend(__sz);                                         \
    if (__ptr)                                                                 \
      balls_debug_tag(__ptr);                                                  \
    __ptr;                                                                     \
  })
