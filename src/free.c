#include "../include/qol.h"
#include "../include/slab.h"
#include <stdint.h>

void cock(void *pp) {
  if (!pp)
    return;

  obj_header_t *hdr = (obj_header_t *)((uint8_t *)pp - sizeof(obj_header_t));
#if SIGMA_DEBUG
  hdr->alloc_file = NULL;
  hdr->alloc_func = NULL;
  hdr->alloc_line = 0;
#endif
  var *slab = hdr->slab;
  var *cache = slab->owner;

  bool was_full = (slab->used == slab->capacity);
  var *node = (free_node_t *)pp;

  node->next = slab->free_list;
  slab->free_list = node;

  slab->used--;

  if (was_full) {
    slab_remove(&cache->full, slab);
    slab_push(&cache->partial, slab);
  }

  if (slab->used == 0) {
    slab_remove(&cache->partial, slab);
    slab_push(&cache->empty, slab);
  }
}
