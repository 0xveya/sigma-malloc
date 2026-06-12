#include "../include/buddy.h"
#include "../include/qol.h"
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

buddy_pool_t *buddy_pool_create(buddy_pool_t *pool, void *raw_mem,
                                usize pool_size) {
  pool->memory_start = raw_mem;
  pool->memory_end = (void *)((uptr)raw_mem + pool_size);

  for (usize i = 0; i < BUDDY_NUM_ORDERS; i++) {
    pool->free_lists[i] = NULL;
  }

  var max_leaves = pool_size / PAGE_SIZE;
  var total_nodes = (2 * max_leaves) - 1;
  pool->bitmap_size = ALIGN_UP((total_nodes + 7) / 8, 8);

  pool->tree_bitmap = (u8 *)raw_mem;
  memset(pool->tree_bitmap, 0, pool->bitmap_size);

  var *usable_mem =
      (void *)ALIGN_UP((uptr)raw_mem + pool->bitmap_size, PAGE_SIZE);

  pool->usable_start = usable_mem;
  var *root_block = (buddy_block_t *)usable_mem;
  root_block->next = NULL;
  root_block->prev = NULL;

  pool->free_lists[BUDDY_NUM_ORDERS - 1] = root_block;

  return pool;
}

static inline usize size_to_order(usize size) {
  usize total_size = size + sizeof(buddy_header_t);

  if (total_size <= PAGE_SIZE) {
    return 0;
  }

  if (total_size > BUDDY_POOL_SIZE) {
    return 6767;
  }

  //  find the next highest power of two
  usize power = (usize)(32U - (u32)__builtin_clz((u32)total_size - 1));

  return power - BUDDY_MIN_ORDER;
}

static inline usize ptr_to_node_index(buddy_pool_t *pool, void *ptr,
                                      usize order) {
  usize block_size = 1ULL << (order + BUDDY_MIN_ORDER);

  usize offset = (uptr)ptr - (uptr)pool->usable_start;

  usize block_index_in_row = offset / block_size;

  usize depth = (BUDDY_NUM_ORDERS - 1) - order;
  usize layer_start_node = (1ULL << depth) - 1;

  return layer_start_node + block_index_in_row;
}

static inline bool get_bitmap_bit(buddy_pool_t *pool, usize node_index) {
  usize byte_idx = node_index / 8;
  usize bit_idx = node_index % 8;
  return (pool->tree_bitmap[byte_idx] >> bit_idx) & 1;
}

static inline void flip_bitmap_bit(buddy_pool_t *pool, usize node_index) {
  usize byte_idx = node_index / 8;
  usize bit_idx = node_index % 8;
  pool->tree_bitmap[byte_idx] ^= (1 << bit_idx);
}

static inline void list_remove(buddy_block_t **head, buddy_block_t *block) {
  if (block->prev) {
    block->prev->next = block->next;
  }
  if (block->next) {
    block->next->prev = block->prev;
  }
  if (*head == block) {
    *head = block->next;
  }
  block->next = NULL;
  block->prev = NULL;
}

static inline void list_push(buddy_block_t **head, buddy_block_t *block) {
  block->next = *head;
  block->prev = NULL;
  if (*head) {
    (*head)->prev = block;
  }
  *head = block;
}

void *buddy_alloc_internal(buddy_pool_t *pool, usize size, const char *file,
                           const char *func, i32 line) {
  void *ptr = buddy_alloc(pool, size);

  if (!ptr)
    return NULL;

#if SIGMA_DEBUG
  buddy_header_t *hdr = (buddy_header_t *)((u8 *)ptr - sizeof(buddy_header_t));

  hdr->alloc_file = file;
  hdr->alloc_func = func;
  hdr->alloc_line = line;
#else
  (void)file;
  (void)func;
  (void)line;
#endif

  return ptr;
}

void *buddy_alloc(buddy_pool_t *pool, usize size) {
  var target_order = size_to_order(size);
  if (target_order >= BUDDY_NUM_ORDERS) {
    printf("target order too large: %zu\n", target_order);
    return NULL;
  }
  printf("target order: %zu\n", target_order);
  var current_order = target_order;
  while (current_order < BUDDY_NUM_ORDERS &&
         pool->free_lists[current_order] == NULL) {
    current_order++;
  }
  if (current_order == BUDDY_NUM_ORDERS) {
    // shit is fucked and no memory
    return NULL;
  }
  var *block = pool->free_lists[current_order];
  list_remove(&pool->free_lists[current_order], block);
  while (current_order > target_order) {
    // get the tree location at the current tier before descending
    var node_idx = ptr_to_node_index(pool, block, current_order);
    flip_bitmap_bit(pool, node_idx); // Mark this exact node as split

    current_order--;

    //  chop block in half
    usize block_size = 1 << (current_order + BUDDY_MIN_ORDER);
    var *buddy = (buddy_block_t *)((uptr)block + block_size);

    // track the idle right half
    list_push(&pool->free_lists[current_order], buddy);
  }
  // flip leaf bit to mark allocated
  var final_node = ptr_to_node_index(pool, block, target_order);
  flip_bitmap_bit(pool, final_node);
  buddy_header_t *header = (buddy_header_t *)block;
  header->order = (u8)target_order;
  header->magic = BUDDY_MAGIC;
  return (void *)((uptr)block + sizeof(buddy_header_t));
}
