#include "../include/arena_allocator.h"

#include "../include/common.h"
#include "../include/libc_wrappers.h"

typedef struct allocator_arena_block {
  struct allocator_arena_block *next;
  usize allocation_size;
  usize used;
  usize capacity;
  u8 data[];
} allocator_arena_block_t;

static allocator_arena_block_t *arena_new_block(allocator_arena_t *arena,
                                                usize minimum_capacity,
                                                const char *file,
                                                const char *func, i32 line) {
  usize capacity = arena->block_size;
  if (capacity < minimum_capacity)
    capacity = minimum_capacity;
  if (capacity > SIZE_MAX - sizeof(allocator_arena_block_t))
    return NULL;

  usize allocation_size = sizeof(allocator_arena_block_t) + capacity;
  allocator_arena_block_t *block = allocator_alloc_aligned_debug(
      arena->parent, allocation_size, _Alignof(max_align_t), file, func, line);
  if (block == NULL)
    return NULL;

  *block = (allocator_arena_block_t){
      .next = arena->blocks,
      .allocation_size = allocation_size,
      .used = 0,
      .capacity = capacity,
  };
  arena->blocks = block;
  return block;
}

static void *arena_alloc_internal(void *ctx, usize size, usize alignment,
                                  const char *file, const char *func,
                                  i32 line) {
  allocator_arena_t *arena = ctx;
  if (arena == NULL || size == 0 || !sigma_alignment_is_valid(alignment) ||
      size > SIZE_MAX - (alignment - 1))
    return NULL;

  allocator_arena_block_t *block = arena->blocks;
  if (block != NULL) {
    uptr current = (uptr)block->data + block->used;
    uptr aligned = ALIGN_UP(current, alignment);
    usize padding = (usize)(aligned - current);
    if (padding <= block->capacity - block->used &&
        size <= block->capacity - block->used - padding) {
      block->used += padding + size;
      return (void *)aligned;
    }
  }

  usize minimum_capacity = size + alignment - 1;
  block = arena_new_block(arena, minimum_capacity, file, func, line);
  if (block == NULL)
    return NULL;

  uptr aligned = ALIGN_UP((uptr)block->data, alignment);
  block->used = (usize)(aligned - (uptr)block->data) + size;
  return (void *)aligned;
}

static void *arena_alloc(void *ctx, usize size, usize alignment) {
  return arena_alloc_internal(ctx, size, alignment, NULL, NULL, 0);
}

static void *arena_alloc_debug(void *ctx, usize size, usize alignment,
                               const char *file, const char *func, i32 line) {
  return arena_alloc_internal(ctx, size, alignment, file, func, line);
}

static void *arena_calloc_debug(void *ctx, usize count, usize size,
                                usize alignment, const char *file,
                                const char *func, i32 line) {
  if (size != 0 && count > SIZE_MAX / size)
    return NULL;
  usize total = count * size;
  void *ptr = arena_alloc_internal(ctx, total, alignment, file, func, line);
  if (ptr != NULL)
    fill_zero(ptr, total);
  return ptr;
}

static void *arena_realloc(void *ctx, void *ptr, usize old_size, usize new_size,
                           usize alignment) {
  if (ptr == NULL)
    return arena_alloc_internal(ctx, new_size, alignment, NULL, NULL, 0);
  if (new_size == 0)
    return NULL;

  void *new_ptr = arena_alloc_internal(ctx, new_size, alignment, NULL, NULL, 0);
  if (new_ptr == NULL)
    return NULL;
  sigma_memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
  return new_ptr;
}

static void *arena_realloc_debug(void *ctx, void *ptr, usize old_size,
                                 usize new_size, usize alignment,
                                 const char *file, const char *func, i32 line) {
  if (ptr == NULL)
    return arena_alloc_internal(ctx, new_size, alignment, file, func, line);
  if (new_size == 0)
    return NULL;

  void *new_ptr =
      arena_alloc_internal(ctx, new_size, alignment, file, func, line);
  if (new_ptr == NULL)
    return NULL;
  sigma_memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
  return new_ptr;
}

static void arena_free(void *ctx, void *ptr, usize size, usize alignment) {
  (void)ctx;
  (void)ptr;
  (void)size;
  (void)alignment;
}

static const allocator_vtable_t g_arena_vtable = {
    .alloc = arena_alloc,
    .alloc_debug = arena_alloc_debug,
    .calloc = NULL,
    .calloc_debug = arena_calloc_debug,
    .realloc = arena_realloc,
    .realloc_debug = arena_realloc_debug,
    .free = arena_free,
};

void allocator_arena_init(allocator_arena_t *arena, allocator_t parent,
                          usize block_size) {
  *arena = (allocator_arena_t){
      .parent = parent,
      .blocks = NULL,
      .block_size = block_size,
  };
}

allocator_t allocator_arena(allocator_arena_t *arena) {
  return (allocator_t){
      .ctx = arena,
      .vtable = &g_arena_vtable,
  };
}

void allocator_arena_reset(allocator_arena_t *arena) {
  allocator_arena_block_t *block = arena->blocks;
  while (block != NULL) {
    allocator_arena_block_t *next = block->next;
    allocator_free_aligned(arena->parent, block, block->allocation_size,
                           _Alignof(max_align_t));
    block = next;
  }
  arena->blocks = NULL;
}

void allocator_arena_deinit(allocator_arena_t *arena) {
  allocator_arena_reset(arena);
}
