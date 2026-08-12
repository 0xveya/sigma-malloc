#pragma once

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
