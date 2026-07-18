#pragma once

/*
 * AI-generated diagram (the author was lazy): per-arena buddy pool.
 *
 * extent backing mapping
 * ┌──────────── tree metadata ────────────┬──── usable buddy blocks ──────┐
 * │ [4 MiB] split/free/full binary tree    │ order 22: one 4 MiB block    │
 * │                                        │ order 21: two 2 MiB blocks   │
 * │                                        │ ... order 12: 4 KiB blocks   │
 * └────────────────────────────────────────┴──────────────────────────────┘
 *
 * free block:      [ buddy_block_t next/prev | unused space ]
 * allocated block: [ buddy_header_t owner arena/pool/order | user payload ]
 *                                                ^
 *                                                returned pointer
 *
 * Buddy serves 1025-byte through near-4-MiB requests.  It also supplies
 * 16-KiB regions to slabs. Each arena owns its pool; remote frees are queued
 * for the owning thread rather than mutating its pool concurrently.
 */

#include "common.h"
#include "debug.h"
#include "qol.h"
#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096

#define BUDDY_MIN_ORDER 12 // 2^12 = 4096 Bytes (PAGE_SIZE)
#define BUDDY_MAX_ORDER 22 // 2^22 = 4 MB
#define BUDDY_NUM_ORDERS (BUDDY_MAX_ORDER - BUDDY_MIN_ORDER + 1) // 11 Orders
#define BUDDY_POOL_SIZE (4 * 1024 * 1024)                        // 4 MB

typedef enum buddy_node_state {
  BUDDY_NODE_FREE,
  BUDDY_NODE_SPLIT,
  BUDDY_NODE_FULL
} buddy_node_state_t;

#define BUDDY_TREE_NODE_COUNT ((2 * (BUDDY_POOL_SIZE / PAGE_SIZE)) - 1)
#define BUDDY_TREE_SIZE                                                        \
  ALIGN_UP(BUDDY_TREE_NODE_COUNT * sizeof(buddy_node_state_t), PAGE_SIZE)
#define BUDDY_BACKING_SIZE (BUDDY_TREE_SIZE + BUDDY_POOL_SIZE)

typedef struct arena arena_t;
typedef struct buddy_pool buddy_pool_t;

typedef struct buddy_header {
#if SIGMA_DEBUG
  const char *alloc_file;
  const char *alloc_func;
  i32 alloc_line;
#endif
  arena_t *arena;
  buddy_pool_t *pool;
  bool is_slab_region;
  u8 order;
  alloc_header_t header;
} buddy_header_t;

typedef struct buddy_block {
  struct buddy_block *next;
  struct buddy_block *prev;
} buddy_block_t;

struct buddy_pool {
  void *memory_start; // ptr returned by the original mmap
  void *memory_end;
  buddy_node_state_t *tree;

  // free lists for each order block size
  buddy_block_t *free_lists[BUDDY_NUM_ORDERS];

  void *usable_start;
};

#if SIGMA_DEBUG
#define buddy_alloc_debug(pool, size)                                          \
  buddy_alloc_internal(pool, size, __FILE__, __func__, __LINE__)
#else
#define buddy_alloc_debug(pool, size)                                          \
  buddy_alloc_internal(pool, size, NULL, NULL, 0)
#endif

buddy_pool_t *buddy_pool_create(buddy_pool_t *pool, void *raw_mem,
                                usize pool_size);
void *buddy_alloc_internal(buddy_pool_t *pool, usize size, const char *file,
                           const char *func, i32 line);
void *buddy_alloc(buddy_pool_t *pool, usize size);
void buddy_free(buddy_pool_t *pool, void *ptr);
void *buddy_alloc_internal(buddy_pool_t *pool, usize size, const char *file,
                           const char *func, i32 line);
