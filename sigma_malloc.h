#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#define var auto

#define PAGE_SIZE 4096
#define SLAB_SIZE (PAGE_SIZE * 4)

#define MAX_SLAB_SIZE 1024
#define NUM_CACHES 8

#ifndef USE_DEBUG_ALLOC
#define USE_DEBUG_ALLOC 0
#endif

#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

static const size_t g_size_classes[NUM_CACHES] = {16,  32,  64,  128,
                                                  256, 512, 768, 1024};

static inline void sigma_defer_cleanup(void (^*block)(void)) {
  if (*block) {
    (*block)();
  }
}

// defer macro using clang blocks extension
#define defer_concat(a, b) a##b
#define defer_id(a) defer_concat(__defer_blk_, a)

#define defer                                                                  \
  void (^defer_id(__LINE__))(void)                                             \
      __attribute__((cleanup(sigma_defer_cleanup))) = ^

typedef struct slab slab_t;
typedef struct cache cache_t;

// freelist node (stored inside free objects)
typedef struct free_node {
  struct free_node *next;
} free_node_t;

// object header (stored before user pointer)
typedef struct obj_header {
  slab_t *slab;
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

// global allocator
typedef struct allocator {
  bool initialized;
  bool is_debug;

  cache_t caches[NUM_CACHES];
} allocator_t;

static allocator_t g_alloc;

void *balls(size_t size);
void cock(void *pp);
