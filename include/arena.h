#pragma once

#include "buddy.h"
#include "slab.h"

#include <stdatomic.h>
#include <stdbool.h>

#define ARENA_EXTENT_SIZE BUDDY_BACKING_SIZE
#define ARENA_REMOTE_DRAIN_INTERVAL 64

typedef struct arena arena_t;
typedef struct arena_extent arena_extent_t;

typedef struct remote_free_node {
  struct remote_free_node *next;
} remote_free_node_t;

struct arena_extent {
  arena_extent_t *next;

  void *mapping;
  usize mapping_size;

  buddy_pool_t buddy;
};

struct arena {
  cache_t caches[NUM_CACHES];

  arena_extent_t *extents;
  arena_extent_t *active_extent;

  _Atomic(remote_free_node_t *) remote_frees;

  usize alloc_counter;
  bool active;

#if SIGMA_DEBUG
  usize allocation_count;
  usize free_count;
  _Atomic(usize) remote_free_count;
#endif
};

arena_t *arena_get(void);
arena_t *arena_get_existing(void);

arena_t *arena_create(void);
bool arena_expand(arena_t *arena);

void *arena_alloc(arena_t *arena, usize size);
void arena_free_buddy_local(arena_t *arena, void *ptr);
void arena_free_local(arena_t *arena, void *ptr);

void *arena_alloc_buddy_region(arena_t *arena, usize size);
void *arena_alloc_slab_region(arena_t *arena, arena_extent_t **out_extent);
void arena_drain_remote_frees(arena_t *arena);
void arena_remote_free(arena_t *arena, void *ptr);

void arena_free_slab_region(arena_t *arena, arena_extent_t *extent, void *ptr);
