#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#define var auto

#define ANSI_RED "\x1b[31m"
#define ANSI_DIM "\x1b[2m"
#define ANSI_BOLD "\x1b[1m"
#define ANSI_RESET "\x1b[0m"

#define PAGE_SIZE 4096
#define SLAB_SIZE (PAGE_SIZE * 4)

#define MAX_SLAB_SIZE 1024
#define NUM_CACHES 8

#ifndef USE_DEBUG_ALLOC
#define USE_DEBUG_ALLOC 0
#endif

#ifndef NO_LEAK_REWARD
#define NO_LEAK_REWARD 0
#endif

#ifndef HORNY_MODE
#define HORNY_MODE 0
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

typedef enum {
  READ_SUCCESS,
  READ_EOF,
  READ_IO_ERROR,
  READ_LINE_NOT_FOUND
} ReadStatus;

// i should have the balls to use my own malloc in my malloc
typedef struct {
  ReadStatus status;
  char line[1024];
  char type[64];
} StackLineResult;

typedef struct {
  ReadStatus status;
  char *type;

  union {
    char *line;
    int os_errno;
  } value;
} ReadLineResult;

typedef struct slab slab_t;
typedef struct cache cache_t;

// freelist node (stored inside free objects)
typedef struct free_node {
  struct free_node *next;
} free_node_t;

// object header (stored before user pointer)
typedef struct obj_header {
  slab_t *slab;
  // #if USE_DEBUG_ALLOC != 2
  const char *alloc_file;
  const char *alloc_func;
  int alloc_line;
  // #endif
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

extern allocator_t g_alloc;

void *balls_backend(size_t size);
void cock(void *pp);

#define balls(size)                                                            \
  __extension__({                                                              \
    size_t __sz = (size);                                                      \
    void *__ptr = balls_backend(__sz);                                         \
                                                                               \
    if (__ptr && g_alloc.initialized && g_alloc.is_debug) {                    \
      obj_header_t *__hdr =                                                    \
          (obj_header_t *)((uint8_t *)__ptr - sizeof(obj_header_t));           \
      __hdr->alloc_file = __FILE__;                                            \
      __hdr->alloc_func = __func__;                                            \
      __hdr->alloc_line = __LINE__;                                            \
    }                                                                          \
    __ptr;                                                                     \
  })
