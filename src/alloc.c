#include "../include/buddy.h"
#include "../include/debug.h"
#include "../include/sigma_malloc.h"
#include "../include/slab.h"
#include <sys/mman.h>

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
  void *raw_buddy_mem = mmap(NULL, BUDDY_POOL_SIZE, PROT_READ | PROT_WRITE,
                             MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  if (raw_buddy_mem == MAP_FAILED) {
    return;
  }

  buddy_pool_create(&g_alloc.buddy_pool, raw_buddy_mem, BUDDY_POOL_SIZE);

  g_alloc.initialized = true;
}

void *balls_debug_backend(size_t size, const char *file, const char *func,
                          int line) {
  void *ptr = balls_backend(size);

  if (!ptr)
    return NULL;

  if (size > MAX_SLAB_OBJ_SIZE) {
    buddy_header_t *hdr =
        (buddy_header_t *)((uint8_t *)ptr - sizeof(buddy_header_t));

    hdr->alloc_file = file;
    hdr->alloc_func = func;
    hdr->alloc_line = line;
  }

  return ptr;
}
void *balls_backend(size_t size) {
  if (!g_alloc.initialized)
    allocator_init();
  if (size <= MAX_SLAB_OBJ_SIZE)
    return slab_alloc(size);
  return buddy_alloc(&g_alloc.buddy_pool, size);
}
