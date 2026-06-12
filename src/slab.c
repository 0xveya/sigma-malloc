#include "../include/slab.h"
#include "../include/qol.h"
#include "../include/sigma_malloc.h"
#include "../include/utils.h"
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

static cache_t *get_cache(size_t size) {
  for (size_t i = 0; i < NUM_CACHES; i++) {
    if (size <= g_alloc.caches[i].obj_size)
      return &g_alloc.caches[i];
  }

  return NULL;
}

static slab_t *slab_create(cache_t *cache) {
  size_t data_size = ALIGN_UP(cache->obj_size, sizeof(void *));

  size_t slot_size = sizeof(obj_header_t) + data_size;

  var *mem = mmap(NULL, SLAB_SIZE, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED)
    return NULL;

  var *slab = (slab_t *)mem;
  slab->owner = cache;
  slab->prev = NULL;
  slab->next = NULL;
  slab->used = 0;
  slab->free_list = NULL;

  uint8_t *obj_start =
      (uint8_t *)mem + ALIGN_UP(sizeof(slab_t), sizeof(void *));
  size_t usable = SLAB_SIZE - (size_t)(obj_start - (uint8_t *)mem);

  slab->capacity = usable / slot_size;

  for (size_t i = 0; i < slab->capacity; i++) {
    uint8_t *slot_ptr = obj_start + (i * slot_size);

    var *hdr = (obj_header_t *)slot_ptr;
#if SIGMA_DEBUG
    hdr->alloc_file = NULL;
    hdr->alloc_func = NULL;
    hdr->alloc_line = 0;
#endif
    hdr->slab = slab;
    hdr->header.magic = SLAB_MAGIC;
    hdr->header.type = ALLOC_TYPE_SLAB;

    free_node_t *node = (free_node_t *)(slot_ptr + sizeof(obj_header_t));

    node->next = slab->free_list;
    slab->free_list = node;
  }

  return slab;
}

static inline void slab_push(slab_t **head, slab_t *slab) {
  slab->prev = NULL;
  slab->next = *head;

  if (*head)
    (*head)->prev = slab;

  *head = slab;
}

static inline void slab_remove(slab_t **head, slab_t *slab) {
  if (slab->prev)
    slab->prev->next = slab->next;

  if (slab->next)
    slab->next->prev = slab->prev;

  if (*head == slab)
    *head = slab->next;

  slab->prev = NULL;
  slab->next = NULL;
}

void *slab_alloc(size_t size) {
  var *cache = get_cache(size);
  if (!cache)
    return NULL;
  var *slab = cache->partial;
  if (!slab) {
    slab = slab_create(cache);

    if (!slab)
      return NULL;

    slab_push(&cache->partial, slab);
  }

  var *node = slab->free_list;

  slab->free_list = node->next;

  slab->used++;

  if (slab->used == slab->capacity) {
    slab_remove(&cache->partial, slab);
    slab_push(&cache->full, slab);
  }

  return node;
}

void slab_free(void *pp) {
  if (!pp)
    return;

  obj_header_t *hdr = (obj_header_t *)((u8 *)pp - sizeof(obj_header_t));

  alloc_header_t *ah = &hdr->header;

  if (ah->magic != SLAB_MAGIC)
    panic("slab_free: invalid magic (double free or corruption)");

  if (ah->type != ALLOC_TYPE_SLAB)
    panic("slab_free: type mismatch");

  ah->magic = 0;

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
