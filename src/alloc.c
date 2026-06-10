#include "../include/debug.h"
#include "../include/sigma_malloc.h"
#include "../include/slab.h"

allocator_t g_alloc = {0};

static void allocator_init(void) {
  if (g_alloc.initialized)
    return;

#if SIGMA_DEBUG
  g_alloc.is_debug = true;
#else
  g_alloc.is_debug = false;
#endif

  for (size_t i = 0; i < NUM_CACHES; i++) {
    g_alloc.caches[i].obj_size = g_size_classes[i];

    g_alloc.caches[i].partial = NULL;
    g_alloc.caches[i].full = NULL;
    g_alloc.caches[i].empty = NULL;
  }

  g_alloc.initialized = true;
}

void *balls_backend(size_t size) {
  if (!g_alloc.initialized)
    allocator_init();
  if (size <= MAX_SLAB_SIZE)
    return slab_alloc(size);

  return NULL;
}
