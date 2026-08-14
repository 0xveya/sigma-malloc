#pragma once

#include "common.h"
#include "qol.h"

typedef struct large_node {
  struct large_node *prev;
  struct large_node *next;

  void *backing;
  usize backing_size;
  struct sigma_allocator *allocator;
  void *user;
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

typedef struct sigma_allocator sigma_allocator_t;

void *large_alloc(sigma_allocator_t *allocator, usize size, usize alignment);
void large_debug_list_lock(void);
void large_debug_list_unlock(void);
void large_free(sigma_allocator_t *allocator, void *ptr);
