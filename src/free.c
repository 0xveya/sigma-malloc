#include "../include/arena.h"
#include "../include/buddy.h"
#include "../include/large.h"
#include "../include/sigma_malloc.h"
#include "../include/slab.h"
#include "../include/utils.h"
#include <stdint.h>

void cock(void *pp) {
  if (!pp)
    return;

  alloc_header_t *ah = alloc_header_from_user(pp);

  switch (ah->type) {
  case ALLOC_TYPE_SLAB: {
    obj_header_t *header = SIGMA_CONTAINER_OF(ah, obj_header_t, header);
    slab_t *slab = header->slab;
    arena_t *owner = slab->arena;
    arena_t *current = arena_get_existing();

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
    arena_t *current = arena_get_existing();
    if (current == owner) {
      arena_free_buddy_local(owner, pp);
    } else {
      arena_remote_free(owner, pp);
    }
    break;
  }

  case ALLOC_TYPE_LARGE:
    large_free(pp);
    break;

  default:
    panic("invalid pointer");
  }
}
