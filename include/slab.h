#pragma once

/*
 * AI-generated diagram (the author was lazy): per-thread slab allocation.
 *
 * arena cache (one size class)              one SLAB_SIZE region (16 KiB)
 * ┌───────────────┐                         ┌─────────────────────────────┐
 * │ partial slabs ├────────────────────────►│ slab_t                      │
 * │ full slabs    │                         ├──────────────┬──────────────┤
 * └───────────────┘                         │ obj header   │ user payload │
 *                                           │ { slab * }   │ free_node *  │
 *                                           ├──────────────┼──────────────┤
 *                                           │ obj header   │ user payload │
 *                                           └──────────────┴──────────────┘
 *
 * Local free:  payload -> slab free list
 * Remote free: payload -> owner arena's atomic remote stack -> local free list
 * Empty slab:  slab_t + stored extent -> owning buddy pool
 */

#include "common.h"
#include "debug.h"
#include "qol.h"
#include <stddef.h>

#define PAGE_SIZE 4096
#define SLAB_SIZE (PAGE_SIZE * 4)

#define MAX_SLAB_OBJ_SIZE 1024
#define NUM_CACHES 8

typedef struct arena arena_t;
typedef struct arena_extent arena_extent_t;

extern const usize g_size_classes[NUM_CACHES];

typedef struct slab slab_t;
typedef struct cache cache_t;

// freelist node (stored inside free objects)
typedef struct free_node {
  struct free_node *next;
} free_node_t;

// object header (stored before user pointer)
typedef struct obj_header {
  slab_t *slab;
#if SIGMA_DEBUG
  const char *alloc_file;
  const char *alloc_func;
  i32 alloc_line;
  i32 padding;
#endif
  alloc_header_t header;
} obj_header_t;

// cache = one size class
typedef struct cache {
  usize obj_size;

  slab_t *partial;
  slab_t *full;
  slab_t *empty;
} cache_t;

// slab = one mmap region
typedef struct slab {
  struct slab *prev;
  struct slab *next;

  arena_t *arena;
  arena_extent_t *extent;
  cache_t *owner;

  usize used;
  usize capacity;

  free_node_t *free_list;
} slab_t;

void *slab_alloc(arena_t *arena, usize size);
void slab_free_local(arena_t *arena, void *ptr);
