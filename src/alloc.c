#include "../include/arena.h"
#include "../include/debug.h"
#include "../include/large.h"
#include "../include/sigma_malloc.h"
#include "../include/slab.h"
#include <stddef.h>

allocator_t g_alloc = {.initialized = true, .is_debug = SIGMA_DEBUG};

void *balls_debug_backend(usize size, const char *file, const char *func,
                          i32 line) {
  void *ptr = balls_backend(size);

  if (!ptr)
    return NULL;

#if SIGMA_DEBUG
  alloc_header_t *ah = alloc_header_from_user(ptr);
  if (ah->type == ALLOC_TYPE_SLAB) {
    obj_header_t *hdr = SIGMA_CONTAINER_OF(ah, obj_header_t, header);
    hdr->alloc_file = file;
    hdr->alloc_func = func;
    hdr->alloc_line = line;
  } else if (ah->type == ALLOC_TYPE_BUDDY) {
    buddy_header_t *hdr = SIGMA_CONTAINER_OF(ah, buddy_header_t, header);
    hdr->alloc_file = file;
    hdr->alloc_func = func;
    hdr->alloc_line = line;
  } else if (ah->type == ALLOC_TYPE_LARGE) {
    large_header_t *hdr = SIGMA_CONTAINER_OF(ah, large_header_t, header);

    hdr->meta->alloc_file = file;
    hdr->meta->alloc_func = func;
    hdr->meta->alloc_line = line;
  }
#endif

  return ptr;
}

void *balls_backend(usize size) {
  if (size <= MAX_SLAB_OBJ_SIZE) {
    arena_t *arena = arena_get();
    if (arena == NULL) {
      return NULL;
    }
    return arena_alloc(arena, size);
  }

  if (size <= BUDDY_POOL_SIZE - offsetof(buddy_header_t, header) -
                  sizeof(alloc_header_t)) {
    arena_t *arena = arena_get();
    if (arena == NULL) {
      return NULL;
    }
    void *ptr = arena_alloc_buddy_region(arena, size);
    if (ptr != NULL) {
      return ptr;
    }
  }

  return large_alloc(size);
}
