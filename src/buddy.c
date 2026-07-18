#include "../include/buddy.h"
#include "../include/qol.h"
#include "../include/utils.h"
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>

static inline usize floor_log2_usize(usize value) {
  return (usize)((sizeof(usize) * 8U - 1U) - (usize)__builtin_clzl(value));
}

static inline usize ceil_log2_usize(usize value) {
  if (value <= 1)
    return 0;
  return floor_log2_usize(value - 1) + 1;
}

buddy_pool_t *buddy_pool_create(buddy_pool_t *pool, void *raw_mem,
                                usize pool_size) {
  if (pool == NULL || raw_mem == NULL || pool_size < BUDDY_BACKING_SIZE) {
    return NULL;
  }
  pool->memory_start = raw_mem;
  pool->memory_end = (void *)((uptr)raw_mem + pool_size);

  for (usize i = 0; i < BUDDY_NUM_ORDERS; i++) {
    pool->free_lists[i] = NULL;
  }

  pool->tree = (buddy_node_state_t *)raw_mem;
  var *usable_mem = (void *)((uptr)raw_mem + BUDDY_TREE_SIZE);
  usize root_order = BUDDY_NUM_ORDERS - 1;

  for (usize i = 0; i < BUDDY_TREE_NODE_COUNT; i++) {
    pool->tree[i] = BUDDY_NODE_FULL;
  }
  pool->tree[0] = BUDDY_NODE_FREE;

  pool->usable_start = usable_mem;
  var *root_block = (buddy_block_t *)usable_mem;
  root_block->next = NULL;
  root_block->prev = NULL;

  pool->free_lists[root_order] = root_block;

  return pool;
}

static inline usize size_to_order(usize size) {
  usize total_size =
      size + offsetof(buddy_header_t, header) + sizeof(alloc_header_t);

  if (total_size <= PAGE_SIZE) {
    return 0;
  }

  if (total_size > BUDDY_POOL_SIZE) {
    return 6767;
  }

  usize power = ceil_log2_usize(total_size);

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

static inline void *node_index_to_ptr(buddy_pool_t *pool, usize node,
                                      usize order) {
  usize depth = (BUDDY_NUM_ORDERS - 1) - order;
  usize first = (1ULL << depth) - 1;
  usize block_index = node - first;
  usize block_size = 1ULL << (order + BUDDY_MIN_ORDER);

  return (void *)((uptr)pool->usable_start + block_index * block_size);
}

static inline usize node_parent(usize node) { return (node - 1) / 2; }

static inline usize node_left(usize node) { return node * 2 + 1; }

static inline usize node_right(usize node) { return node * 2 + 2; }

static inline usize node_buddy(usize node) {
  return (node & 1) ? node + 1 : node - 1;
}

static inline bool node_is_free(buddy_pool_t *pool, usize node) {
  return pool->tree[node] == BUDDY_NODE_FREE;
}

static inline bool node_is_split(buddy_pool_t *pool, usize node) {
  return pool->tree[node] == BUDDY_NODE_SPLIT;
}

static inline void node_mark_free(buddy_pool_t *pool, usize node) {
  pool->tree[node] = BUDDY_NODE_FREE;
}

static inline void node_mark_split(buddy_pool_t *pool, usize node) {
  pool->tree[node] = BUDDY_NODE_SPLIT;
}

static inline void node_mark_full(buddy_pool_t *pool, usize node) {
  pool->tree[node] = BUDDY_NODE_FULL;
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
  alloc_header_t *ah = alloc_header_from_user(ptr);
  buddy_header_t *hdr = SIGMA_CONTAINER_OF(ah, buddy_header_t, header);

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
    return NULL;
  }

  var current_order = target_order;

  while (current_order < BUDDY_NUM_ORDERS &&
         pool->free_lists[current_order] == NULL) {
    current_order++;
  }

  if (current_order == BUDDY_NUM_ORDERS)
    return NULL;

  var *block = pool->free_lists[current_order];
  list_remove(&pool->free_lists[current_order], block);

  while (current_order > target_order) {

    var node_idx = ptr_to_node_index(pool, block, current_order);
    node_mark_split(pool, node_idx);

    current_order--;

    usize left = node_left(node_idx);
    usize right = node_right(node_idx);
    node_mark_free(pool, left);
    node_mark_free(pool, right);

    usize block_size = 1ULL << (current_order + BUDDY_MIN_ORDER);
    var *buddy = (buddy_block_t *)((uptr)block + block_size);

    list_push(&pool->free_lists[current_order], buddy);
  }

  var final_node = ptr_to_node_index(pool, block, target_order);
  node_mark_full(pool, final_node);

  buddy_header_t *header = (buddy_header_t *)block;
  header->order = (u8)target_order;
  header->arena = NULL;
  header->pool = pool;
  header->is_slab_region = false;
  header->header.magic = BUDDY_MAGIC;
  header->header.type = ALLOC_TYPE_BUDDY;

  return (void *)((u8 *)&header->header + sizeof(alloc_header_t));
}

void buddy_free(buddy_pool_t *pool, void *pp) {
  if (!pp)
    return;

  alloc_header_t *ah = alloc_header_from_user(pp);
  buddy_header_t *hdr = SIGMA_CONTAINER_OF(ah, buddy_header_t, header);

  if (ah->magic != BUDDY_MAGIC)
    panic("buddy_free: invalid magic (double free/corruption)");

  if (ah->type != ALLOC_TYPE_BUDDY)
    panic("buddy_free: type mismatch");

  usize order = hdr->order;

  usize node_index = ptr_to_node_index(pool, hdr, order);
  node_mark_free(pool, node_index);
  buddy_block_t *block = (buddy_block_t *)hdr;

  while (order < BUDDY_NUM_ORDERS - 1) {
    usize buddy = node_buddy(node_index);

    if (!node_is_free(pool, buddy))
      break;

    if (node_is_split(pool, buddy))
      break;

    buddy_block_t *buddy_block =
        (buddy_block_t *)node_index_to_ptr(pool, buddy, order);

    list_remove(&pool->free_lists[order], buddy_block);

    usize parent = node_parent(node_index);

    node_mark_free(pool, node_index);
    node_mark_free(pool, buddy);
    node_mark_free(pool, parent);

    if ((uptr)buddy_block < (uptr)block)
      block = buddy_block;

    node_index = parent;
    order++;
  }

  list_push(&pool->free_lists[order], block);

#if SIGMA_DEBUG
  hdr->alloc_file = NULL;
  hdr->alloc_func = NULL;
  hdr->alloc_line = 0;
#endif
}
