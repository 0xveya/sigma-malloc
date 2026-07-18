#include "../include/slab.h"
#include "../include/arena.h"
#include "../include/utils.h"

#include <stddef.h>
#include <string.h>

const usize g_size_classes[NUM_CACHES] = {16, 32, 64, 128, 256, 512, 768, 1024};

static cache_t *get_cache(arena_t *arena, usize size) {
  for (usize i = 0; i < NUM_CACHES; i++) {
    if (size <= arena->caches[i].obj_size) {
      return &arena->caches[i];
    }
  }
  return NULL;
}

static inline void slab_push(slab_t **head, slab_t *slab) {
  slab->prev = NULL;
  slab->next = *head;
  if (*head != NULL) {
    (*head)->prev = slab;
  }
  *head = slab;
}

static inline void slab_remove(slab_t **head, slab_t *slab) {
  if (slab->prev != NULL) {
    slab->prev->next = slab->next;
  }
  if (slab->next != NULL) {
    slab->next->prev = slab->prev;
  }
  if (*head == slab) {
    *head = slab->next;
  }
  slab->prev = NULL;
  slab->next = NULL;
}

static slab_t *slab_create(arena_t *arena, cache_t *cache) {
  arena_extent_t *extent = NULL;
  void *region = arena_alloc_slab_region(arena, &extent);
  if (region == NULL) {
    return NULL;
  }

  usize data_size = ALIGN_UP(cache->obj_size, sizeof(void *));
  usize user_offset = offsetof(obj_header_t, header) + sizeof(alloc_header_t);
  usize slot_size = user_offset + data_size;
  slab_t *slab = region;
  memset(slab, 0, sizeof(*slab));

  slab->arena = arena;
  slab->extent = extent;
  slab->owner = cache;

  u8 *obj_start = (u8 *)slab + ALIGN_UP(sizeof(*slab), sizeof(void *));
  usize usable = SLAB_SIZE - (usize)(obj_start - (u8 *)slab);
  slab->capacity = usable / slot_size;
  if (slab->capacity == 0) {
    arena_free_slab_region(arena, extent, slab);
    return NULL;
  }

  for (usize i = 0; i < slab->capacity; i++) {
    u8 *slot = obj_start + i * slot_size;
    obj_header_t *header = (obj_header_t *)slot;
    header->slab = slab;
    header->header.magic = SLAB_MAGIC;
    header->header.type = ALLOC_TYPE_SLAB;
#if SIGMA_DEBUG
    header->alloc_file = NULL;
    header->alloc_func = NULL;
    header->alloc_line = 0;
#endif
    free_node_t *node = (free_node_t *)(slot + user_offset);
    node->next = slab->free_list;
    slab->free_list = node;
  }
  return slab;
}

void *slab_alloc(arena_t *arena, usize size) {
  cache_t *cache = get_cache(arena, size);
  if (cache == NULL) {
    return NULL;
  }

  slab_t *slab = cache->partial;
  if (slab == NULL) {
    slab = slab_create(arena, cache);
    if (slab == NULL) {
      return NULL;
    }
    slab_push(&cache->partial, slab);
  }

  free_node_t *node = slab->free_list;
  slab->free_list = node->next;
  alloc_header_t *alloc_header = alloc_header_from_user(node);
  obj_header_t *header = SIGMA_CONTAINER_OF(alloc_header, obj_header_t, header);
  header->slab = slab;
  alloc_header->magic = SLAB_MAGIC;
  alloc_header->type = ALLOC_TYPE_SLAB;
  slab->used++;

  if (slab->used == slab->capacity) {
    slab_remove(&cache->partial, slab);
    slab_push(&cache->full, slab);
  }
  return node;
}

void slab_free_local(arena_t *arena, void *ptr) {
  if (arena == NULL || ptr == NULL) {
    return;
  }
  alloc_header_t *alloc_header = alloc_header_from_user(ptr);
  obj_header_t *header = SIGMA_CONTAINER_OF(alloc_header, obj_header_t, header);
  slab_t *slab = header->slab;

  if (alloc_header->magic != SLAB_MAGIC ||
      alloc_header->type != ALLOC_TYPE_SLAB || slab == NULL ||
      slab->arena != arena) {
    panic("slab_free_local: invalid pointer, double free, or wrong arena");
  }
  alloc_header->magic = 0;
#if SIGMA_DEBUG
  header->alloc_file = NULL;
  header->alloc_func = NULL;
  header->alloc_line = 0;
#endif

  cache_t *cache = slab->owner;
  bool was_full = slab->used == slab->capacity;
  free_node_t *node = ptr;
  node->next = slab->free_list;
  slab->free_list = node;
  slab->used--;

  if (was_full) {
    slab_remove(&cache->full, slab);
    slab_push(&cache->partial, slab);
  }
  if (slab->used == 0) {
    arena_t *owner_arena = slab->arena;
    arena_extent_t *extent = slab->extent;
    slab_remove(&cache->partial, slab);
    arena_free_slab_region(owner_arena, extent, slab);
  }
}
