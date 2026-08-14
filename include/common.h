#pragma once

#include "qol.h"
#include <stddef.h>

#define ALIGN_UP(x, a) (((x) + ((size_t)(a) - 1)) & ~((size_t)(a) - 1))

#define SLAB_MAGIC 0x42
#define BUDDY_MAGIC 0x67
#define LARGE_MAGIC 0x69

typedef enum {
  ALLOC_TYPE_SLAB = 1,
  ALLOC_TYPE_BUDDY = 2,
  ALLOC_TYPE_LARGE = 3,
} alloc_type_t;

typedef struct __attribute__((aligned(sizeof(void *)))) alloc_header {
  u8 magic;
  alloc_type_t type;
  usize requested_size;
  usize alignment;
} alloc_header_t;

static inline bool sigma_alignment_is_valid(usize alignment) {
  return alignment != 0 && (alignment & (alignment - 1)) == 0;
}

static inline alloc_header_t *alloc_header_from_user(void *ptr) {
  return (alloc_header_t *)((u8 *)ptr - sizeof(alloc_header_t));
}

#define SIGMA_CONTAINER_OF(ptr, type, member)                                  \
  ((type *)((u8 *)(ptr) - offsetof(type, member)))
