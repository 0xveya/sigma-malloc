#include "sigma_malloc.h"
#include <stdint.h>
#include <stdio.h>

static void allocator_init(void) {
  if (g_alloc.initialized)
    return;

  for (size_t i = 0; i < NUM_CACHES; i++) {
    g_alloc.caches[i].obj_size = g_size_classes[i];

    g_alloc.caches[i].partial = NULL;
    g_alloc.caches[i].full = NULL;
    g_alloc.caches[i].empty = NULL;
  }

  g_alloc.initialized = true;
}

static cache_t *get_cache(size_t size) {
  for (size_t i = 0; i < NUM_CACHES; i++) {
    if (size <= g_alloc.caches[i].obj_size)
      return &g_alloc.caches[i];
  }

  return NULL;
}

static slab_t *slab_create(cache_t *cache) {
  size_t size = ALIGN_UP(cache->obj_size, sizeof(void *));
  size_t size_fr = ALIGN_UP(sizeof(obj_header_t) + size, sizeof(void *));

  void *mem = mmap(NULL, SLAB_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (mem == MAP_FAILED)
    return NULL;

  slab_t *slab = (slab_t *)mem;
  slab->owner = cache;

  slab->prev = NULL;
  slab->next = NULL;

  slab->used = 0;

  slab->free_list = NULL;

  uint8_t *obj_start =
      (uint8_t *)mem + ALIGN_UP(sizeof(slab_t), sizeof(void *));

  size_t usable = SLAB_SIZE - ALIGN_UP(sizeof(slab_t), sizeof(void *));

  slab->capacity = usable / size_fr;

  for (size_t i = 0; i < slab->capacity; i++) {

    obj_header_t *hdr = (obj_header_t *)(obj_start + (i * size_fr));

    hdr->slab = slab;

    free_node_t *node = (free_node_t *)(hdr + 1);

    node->next = slab->free_list;
    slab->free_list = node;
  }

  return slab;
}

static void slab_push(slab_t **head, slab_t *slab) {
  slab->prev = NULL;
  slab->next = *head;

  if (*head)
    (*head)->prev = slab;

  *head = slab;
}

static void slab_remove(slab_t **head, slab_t *slab) {
  if (slab->prev)
    slab->prev->next = slab->next;

  if (slab->next)
    slab->next->prev = slab->prev;

  if (*head == slab)
    *head = slab->next;

  slab->prev = NULL;
  slab->next = NULL;
}

static void *slab_alloc(size_t size) {
  cache_t *cache = get_cache(size);
  if (!cache)
    return NULL;
  slab_t *slab = cache->partial;
  if (!slab) {
    slab = slab_create(cache);

    if (!slab)
      return NULL;

    slab_push(&cache->partial, slab);
  }

  free_node_t *node = slab->free_list;

  slab->free_list = node->next;

  slab->used++;

  if (slab->used == slab->capacity) {
    slab_remove(&cache->partial, slab);
    slab_push(&cache->full, slab);
  }

  return node;
}

void cock(void *pp) {
  if (!pp)
    return;

  obj_header_t *hdr = ((obj_header_t *)pp) - 1;

  slab_t *slab = hdr->slab;

  cache_t *cache = slab->owner;

  bool was_full = (slab->used == slab->capacity);

  free_node_t *node = (free_node_t *)pp;

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

void *balls(size_t size) {
  allocator_init();

  printf("size: %zu\n", size);
  if (size <= MAX_SLAB_SIZE)
    return slab_alloc(size);

  return NULL;
}
