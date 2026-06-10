#pragma once

#include "./debug.h"
#include <stddef.h>

#define SLAB_SIZE (PAGE_SIZE * 4)

#define MAX_SLAB_SIZE 1024
#define NUM_CACHES 8
#define PAGE_SIZE 4096

#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

static const size_t g_size_classes[NUM_CACHES] = {16,  32,  64,  128,
                                                  256, 512, 768, 1024};

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
  int alloc_line;
#endif
} obj_header_t;

// cache = one size class
typedef struct cache {
  size_t obj_size;

  slab_t *partial;
  slab_t *full;
  slab_t *empty;
} cache_t;

// slab = one mmap region
typedef struct slab {
  struct slab *prev;
  struct slab *next;

  cache_t *owner;

  size_t used;
  size_t capacity;

  free_node_t *free_list;
} slab_t;

void *slab_alloc(size_t size);
void slab_push(slab_t **head, slab_t *slab);
void slab_remove(slab_t **head, slab_t *slab);
