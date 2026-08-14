#pragma once

#include "qol.h"

#include "libc_wrappers.h"
#include "utils/bzero.h"
#include <stddef.h>

/* Generic non-owning allocator handle. Copying it does not copy allocator
 * state; ctx continues to point at the concrete implementation. */
typedef struct allocator allocator_t;
typedef struct allocator_vtable allocator_vtable_t;

/* Implementation contract. Normal callers use the helpers below. Sizes and
 * alignments stay explicit so wrappers can compose without hidden metadata. */
struct allocator_vtable {
  void *(*alloc)(void *ctx, usize size, usize alignment);
  void *(*alloc_debug)(void *ctx, usize size, usize alignment, const char *file,
                       const char *func, i32 line);
  void *(*calloc)(void *ctx, usize count, usize size, usize alignment);
  void *(*calloc_debug)(void *ctx, usize count, usize size, usize alignment,
                        const char *file, const char *func, i32 line);

  void *(*realloc)(void *ctx, void *ptr, usize old_size, usize new_size,
                   usize alignment);
  void *(*realloc_debug)(void *ctx, void *ptr, usize old_size, usize new_size,
                         usize alignment, const char *file, const char *func,
                         i32 line);

  void (*free)(void *ctx, void *ptr, usize size, usize alignment);
};

/* Small handle intended to be copied and passed by value. */
struct allocator {
  void *ctx;
  const allocator_vtable_t *vtable;
};

static inline void *allocator_alloc_aligned(allocator_t allocator, usize size,
                                            usize alignment) {
  if (allocator.vtable == NULL || allocator.vtable->alloc == NULL)
    return NULL;

  return allocator.vtable->alloc(allocator.ctx, size, alignment);
}

static inline void *allocator_alloc_aligned_debug(allocator_t allocator,
                                                  usize size, usize alignment,
                                                  const char *file,
                                                  const char *func, i32 line) {
  if (allocator.vtable != NULL && allocator.vtable->alloc_debug != NULL)
    return allocator.vtable->alloc_debug(allocator.ctx, size, alignment, file,
                                         func, line);
  return allocator_alloc_aligned(allocator, size, alignment);
}

static inline void *allocator_realloc_aligned(allocator_t allocator, void *ptr,
                                              usize old_size, usize new_size,
                                              usize alignment) {
  if (allocator.vtable == NULL)
    return NULL;

  /* Prefer a concrete realloc implementation when one is available. */
  if (allocator.vtable->realloc != NULL) {
    return allocator.vtable->realloc(allocator.ctx, ptr, old_size, new_size,
                                     alignment);
  }

  /* Generic allocate-copy-free fallback. */
  if (ptr == NULL) {
    return allocator_alloc_aligned(allocator, new_size, alignment);
  }

  if (new_size == 0) {
    allocator.vtable->free(allocator.ctx, ptr, old_size, alignment);

    return NULL;
  }

  void *new_ptr = allocator_alloc_aligned(allocator, new_size, alignment);

  if (new_ptr == NULL)
    return NULL;

  usize copy_size = old_size < new_size ? old_size : new_size;

  sigma_memcpy(new_ptr, ptr, copy_size);

  allocator.vtable->free(allocator.ctx, ptr, old_size, alignment);

  return new_ptr;
}

static inline void *
allocator_realloc_aligned_debug(allocator_t allocator, void *ptr,
                                usize old_size, usize new_size, usize alignment,
                                const char *file, const char *func, i32 line) {
  if (allocator.vtable != NULL && allocator.vtable->realloc_debug != NULL)
    return allocator.vtable->realloc_debug(
        allocator.ctx, ptr, old_size, new_size, alignment, file, func, line);
  return allocator_realloc_aligned(allocator, ptr, old_size, new_size,
                                   alignment);
}

static inline void allocator_free_aligned(allocator_t allocator, void *ptr,
                                          usize size, usize alignment) {
  if (ptr == NULL)
    return;

  if (allocator.vtable == NULL || allocator.vtable->free == NULL)
    return;

  allocator.vtable->free(allocator.ctx, ptr, size, alignment);
}

/* Allocate raw bytes with the platform's normal maximum alignment. */
static inline void *allocator_malloc(allocator_t allocator, usize size) {
  return allocator_alloc_aligned(allocator, size, _Alignof(max_align_t));
}

/* Overflow-checked zeroed allocation. */
static inline void *allocator_calloc_aligned(allocator_t allocator, usize count,
                                             usize size, usize alignment) {
  if (size != 0 && count > SIZE_MAX / size)
    return NULL;

  usize total = count * size;

  if (allocator.vtable != NULL && allocator.vtable->calloc != NULL)
    return allocator.vtable->calloc(allocator.ctx, count, size, alignment);

  void *ptr = allocator_alloc_aligned(allocator, total, alignment);

  if (ptr != NULL)
    fill_zero(ptr, total);

  return ptr;
}

static inline void *allocator_calloc_aligned_debug(allocator_t allocator,
                                                   usize count, usize size,
                                                   usize alignment,
                                                   const char *file,
                                                   const char *func, i32 line) {
  if (size != 0 && count > SIZE_MAX / size)
    return NULL;
  if (allocator.vtable != NULL && allocator.vtable->calloc_debug != NULL)
    return allocator.vtable->calloc_debug(allocator.ctx, count, size, alignment,
                                          file, func, line);

  usize total = count * size;
  void *ptr = allocator_alloc_aligned_debug(allocator, total, alignment, file,
                                            func, line);
  if (ptr != NULL)
    fill_zero(ptr, total);
  return ptr;
}

static inline void *allocator_calloc(allocator_t allocator, usize count,
                                     usize size) {
  return allocator_calloc_aligned(allocator, count, size,
                                  _Alignof(max_align_t));
}

/*
 * This version requires the old allocation size.
 *
 * That's useful for truly generic allocators, because not every allocator
 * stores allocation sizes in a header.
 */
static inline void *allocator_realloc_sized(allocator_t allocator, void *ptr,
                                            usize old_size, usize new_size) {
  return allocator_realloc_aligned(allocator, ptr, old_size, new_size,
                                   _Alignof(max_align_t));
}

/*
 * Overflow-safe array resize.
 */
static inline void *allocator_reallocarray_sized(allocator_t allocator,
                                                 void *ptr, usize old_count,
                                                 usize new_count,
                                                 usize element_size) {
  if (element_size != 0 && new_count > SIZE_MAX / element_size)
    return NULL;

  if (element_size != 0 && old_count > SIZE_MAX / element_size)
    return NULL;

  usize old_size = old_count * element_size;
  usize new_size = new_count * element_size;

  return allocator_realloc_sized(allocator, ptr, old_size, new_size);
}

/*
 * Generic free needs the size because wrappers such as arenas, fixed buffers,
 * debugging allocators, etc. may care about it.
 *
 * Sigma itself can ignore it if its headers already contain the size.
 */
static inline void allocator_free_sized(allocator_t allocator, void *ptr,
                                        usize size) {
  allocator_free_aligned(allocator, ptr, size, _Alignof(max_align_t));
}

/*
 * Allocate one T.
 *
 * foo_t *foo = allocator_new(alloc, foo_t);
 */
#define allocator_new(allocator, T)                                            \
  ((T *)allocator_alloc_aligned_debug((allocator), sizeof(T), _Alignof(T),     \
                                      __FILE__, __func__, __LINE__))

/*
 * Allocate N T objects.
 *
 * int *values = allocator_array(alloc, int, 128);
 */
#define allocator_array(allocator, T, count)                                   \
  ((T *)allocator_alloc_aligned_debug((allocator), sizeof(T) * (count),        \
                                      _Alignof(T), __FILE__, __func__,         \
                                      __LINE__))

/*
 * Allocate a zeroed array.
 */
#define allocator_array_zeroed(allocator, T, count)                            \
  ((T *)allocator_calloc_aligned_debug((allocator), (count), sizeof(T),        \
                                       _Alignof(T), __FILE__, __func__,        \
                                       __LINE__))

/*
 * Grow/shrink an existing typed array.
 *
 * Example:
 *
 *   int *items = allocator_array(alloc, int, 16);
 *
 *   items = allocator_rearray(
 *       alloc,
 *       items,
 *       16,
 *       32
 *   );
 *
 * C23 typeof means the pointer type is preserved.
 */
#define allocator_rearray(allocator, ptr, old_count, new_count)                \
  ((typeof(ptr))allocator_realloc_aligned_debug(                               \
      (allocator), (ptr), sizeof(*(ptr)) * (old_count),                        \
      sizeof(*(ptr)) * (new_count), _Alignof(typeof(*(ptr))), __FILE__,        \
      __func__, __LINE__))

/*
 * Free one typed object.
 */
#define allocator_delete(allocator, ptr)                                       \
  allocator_free_aligned((allocator), (ptr), sizeof(*(ptr)),                   \
                         _Alignof(typeof(*(ptr))))

/*
 * Free a typed array.
 */
#define allocator_free_array(allocator, ptr, count)                            \
  allocator_free_aligned((allocator), (ptr), sizeof(*(ptr)) * (count),         \
                         _Alignof(typeof(*(ptr))))
