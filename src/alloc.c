#include "../include/arena.h"
#include "../include/debug.h"
#include "../include/large.h"
#include "../include/sigma_malloc.h"
#include "../include/slab.h"
#include <stddef.h>

static void *finish_alloc(void *ptr) {
  if (ptr != NULL)
    sigma_debug_forget_freed(ptr);
  return ptr;
}

void *sigma_alloc_debug(sigma_allocator_t *sigma, usize size, usize alignment,
                        const char *file, const char *func, i32 line) {
  void *ptr = sigma_alloc(sigma, size, alignment);

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

void *sigma_alloc(sigma_allocator_t *sigma, usize size, usize alignment) {
  if (sigma == NULL || !sigma->initialized || sigma->source == NULL ||
      size == 0 || !sigma_alignment_is_valid(alignment))
    return NULL;

  if (size <= MAX_SLAB_OBJ_SIZE && alignment <= SLAB_MAX_ALIGNMENT) {
    arena_t *arena = arena_get(sigma);
    if (arena == NULL) {
      return NULL;
    }
    void *ptr = arena_alloc(arena, size, alignment);
    if (ptr != NULL)
      alloc_header_from_user(ptr)->alignment = alignment;
    return finish_alloc(ptr);
  }

  if (size <= BUDDY_POOL_SIZE && alignment <= BUDDY_POOL_SIZE) {
    arena_t *arena = arena_get(sigma);
    if (arena == NULL) {
      return NULL;
    }
    void *ptr = arena_alloc_buddy_region(arena, size, alignment);
    if (ptr != NULL) {
      return finish_alloc(ptr);
    }
  }

  return finish_alloc(large_alloc(sigma, size, alignment));
}
