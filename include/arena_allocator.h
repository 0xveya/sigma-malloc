#pragma once

#include "allocator_vtable.h"

typedef struct allocator_arena {
  allocator_t parent;
  void *blocks;
  usize block_size;
} allocator_arena_t;

void allocator_arena_init(allocator_arena_t *arena, allocator_t parent,
                          usize block_size);
allocator_t allocator_arena(allocator_arena_t *arena);
void allocator_arena_reset(allocator_arena_t *arena);
void allocator_arena_deinit(allocator_arena_t *arena);
