#pragma once

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
void large_free(void *ptr);
