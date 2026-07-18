#pragma once

/*
 * AI-generated diagram (the author was lazy): large mmap allocation.
 *
 * mmap mapping (requests larger than the buddy maximum)
 * ┌───────────────────────┬───────────────────────────────────────────────┐
 * │ large_header_t        │ user payload                                  │
 * │  - metadata pointer   │                                               │
 * │  - allocation header  │                                               │
 * └───────────────────────┴───────────────────────────────────────────────┘
 *                         ^
 *                         pointer returned to the caller
 *
 * Metadata is allocated from the current thread arena and links the mapping
 * into the debug leak list. The operating system owns the mapping itself;
 * freeing it ends with munmap(mapping, mapping_size).
 */

#include "common.h"
#include "qol.h"

typedef struct large_node {
  struct large_node *prev;
  struct large_node *next;
  void *mmap_ptr;
  usize mmap_size;
} large_node_t;

typedef struct large_metadata {
  large_node_t *node;
  const char *alloc_file;
  const char *alloc_func;
  i32 alloc_line;
} large_metadata_t;

typedef struct large_header {
  large_metadata_t *meta;
  alloc_header_t header;
} large_header_t;

void *large_alloc(usize size);
void large_debug_list_lock(void);
void large_debug_list_unlock(void);
void large_free(void *ptr);
