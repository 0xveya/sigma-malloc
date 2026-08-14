#include "../include/arena.h"
#include "../include/buddy.h"
#include "../include/large.h"
#include "../include/sigma_malloc.h"
#include "../include/slab.h"
#include "../include/utils.h"
#include <stdint.h>

void cock(void *ptr) { sigma_free(&g_alloc, ptr); }

void sigma_free(sigma_allocator_t *sigma, void *pp) {
  if (!pp)
    return;

  if (sigma_debug_check_double_free(pp))
    return;

  alloc_header_t *ah = alloc_header_from_user(pp);

#if SIGMA_DEBUG
  const char *alloc_file = NULL;
  const char *alloc_func = NULL;
  i32 alloc_line = 0;

  if (ah->type == ALLOC_TYPE_SLAB) {
    obj_header_t *header = SIGMA_CONTAINER_OF(ah, obj_header_t, header);
    alloc_file = header->alloc_file;
    alloc_func = header->alloc_func;
    alloc_line = header->alloc_line;
  } else if (ah->type == ALLOC_TYPE_BUDDY) {
    buddy_header_t *header = SIGMA_CONTAINER_OF(ah, buddy_header_t, header);
    alloc_file = header->alloc_file;
    alloc_func = header->alloc_func;
    alloc_line = header->alloc_line;
  } else if (ah->type == ALLOC_TYPE_LARGE) {
    large_header_t *header = SIGMA_CONTAINER_OF(ah, large_header_t, header);
    alloc_file = header->meta->alloc_file;
    alloc_func = header->meta->alloc_func;
    alloc_line = header->meta->alloc_line;
  }
  sigma_debug_record_free(pp, alloc_file, alloc_func, alloc_line);
#endif

  switch (ah->type) {
  case ALLOC_TYPE_SLAB: {
    obj_header_t *header = SIGMA_CONTAINER_OF(ah, obj_header_t, header);
    slab_t *slab = header->slab;
    arena_t *owner = slab->arena;
    if (owner->allocator != sigma)
      panic("sigma_free: allocation belongs to another allocator");
    arena_t *current = arena_get_existing(sigma);

    if (current == owner) {
      arena_free_local(owner, pp);
    } else {
      arena_remote_free(owner, pp);
    }
    break;
  }

  case ALLOC_TYPE_BUDDY: {
    buddy_header_t *header = SIGMA_CONTAINER_OF(ah, buddy_header_t, header);
    arena_t *owner = header->arena;
    if (owner->allocator != sigma)
      panic("sigma_free: allocation belongs to another allocator");
    arena_t *current = arena_get_existing(sigma);
    if (current == owner) {
      arena_free_buddy_local(owner, pp);
    } else {
      arena_remote_free(owner, pp);
    }
    break;
  }

  case ALLOC_TYPE_LARGE:
    large_free(sigma, pp);
    break;

  default:
    panic("invalid pointer");
  }
}
