#include "../include/arena.h"
#include "../include/utils.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>

static _Thread_local arena_t *g_thread_arena = NULL;

void *arena_alloc_slab_region(arena_t *arena, arena_extent_t **out_extent) {
  if (arena == NULL) {
    return NULL;
  }

  arena_extent_t *extent = arena->active_extent;

  if (extent != NULL) {
    void *ptr = buddy_alloc(&extent->buddy, SLAB_SIZE);

    if (ptr != NULL) {
      buddy_header_t *header = SIGMA_CONTAINER_OF(alloc_header_from_user(ptr),
                                                  buddy_header_t, header);
      header->is_slab_region = true;
      if (out_extent != NULL) {
        *out_extent = extent;
      }

      return ptr;
    }
  }

  for (extent = arena->extents; extent != NULL; extent = extent->next) {
    if (extent == arena->active_extent) {
      continue;
    }

    void *ptr = buddy_alloc(&extent->buddy, SLAB_SIZE);

    if (ptr != NULL) {
      arena->active_extent = extent;
      buddy_header_t *header = SIGMA_CONTAINER_OF(alloc_header_from_user(ptr),
                                                  buddy_header_t, header);
      header->is_slab_region = true;

      if (out_extent != NULL) {
        *out_extent = extent;
      }

      return ptr;
    }
  }

  if (!arena_expand(arena)) {
    return NULL;
  }

  extent = arena->active_extent;

  void *ptr = buddy_alloc(&extent->buddy, SLAB_SIZE);

  if (ptr != NULL && out_extent != NULL) {
    *out_extent = extent;
  }
  if (ptr != NULL) {
    buddy_header_t *header =
        SIGMA_CONTAINER_OF(alloc_header_from_user(ptr), buddy_header_t, header);
    header->is_slab_region = true;
  }

  return ptr;
}

void *arena_alloc_buddy_region(arena_t *arena, usize size) {
  if (arena == NULL) {
    return NULL;
  }

  arena_extent_t *extent = arena->active_extent;
  void *ptr = extent == NULL ? NULL : buddy_alloc(&extent->buddy, size);

  if (ptr == NULL) {
    for (extent = arena->extents; extent != NULL; extent = extent->next) {
      if (extent == arena->active_extent) {
        continue;
      }
      ptr = buddy_alloc(&extent->buddy, size);
      if (ptr != NULL) {
        arena->active_extent = extent;
        break;
      }
    }
  }

  if (ptr == NULL && arena_expand(arena)) {
    ptr = buddy_alloc(&arena->active_extent->buddy, size);
  }
  if (ptr != NULL) {
    alloc_header_t *alloc_header = alloc_header_from_user(ptr);
    buddy_header_t *header =
        SIGMA_CONTAINER_OF(alloc_header, buddy_header_t, header);
    header->arena = arena;
  }
  return ptr;
}

arena_t *arena_get_existing(void) { return g_thread_arena; }

arena_t *arena_get(void) {
  if (g_thread_arena != NULL) {
    return g_thread_arena;
  }

  g_thread_arena = arena_create();
  return g_thread_arena;
}

static usize align_up_page(usize size) {
  return (size + (usize)PAGE_SIZE - 1) & ~((usize)PAGE_SIZE - 1);
}

bool arena_expand(arena_t *arena) {
  if (arena == NULL) {
    return false;
  }

  usize metadata_size = align_up_page(sizeof(arena_extent_t));
  usize mapping_size = metadata_size + ARENA_EXTENT_SIZE;

  void *mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (mapping == MAP_FAILED) {
    return false;
  }

  arena_extent_t *extent = mapping;
  memset(extent, 0, sizeof(*extent));

  void *buddy_memory = (unsigned char *)mapping + metadata_size;

  if (buddy_pool_create(&extent->buddy, buddy_memory, ARENA_EXTENT_SIZE) ==
      NULL) {
    munmap(mapping, mapping_size);
    return false;
  }
  extent->mapping = mapping;
  extent->mapping_size = mapping_size;

  extent->next = arena->extents;
  arena->extents = extent;
  arena->active_extent = extent;

  return true;
}

arena_t *arena_create(void) {
  usize mapping_size = align_up_page(sizeof(arena_t));
  void *mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mapping == MAP_FAILED) {
    return NULL;
  }
  arena_t *arena = mapping;
  memset(arena, 0, sizeof(arena_t));
  for (usize i = 0; i < NUM_CACHES; i++) {
    arena->caches[i].obj_size = g_size_classes[i];
  }

  atomic_init(&arena->remote_frees, NULL);
  arena->active = true;

  if (!arena_expand(arena)) {
    munmap(mapping, mapping_size);
    return NULL;
  }

  return arena;
}

static bool pointer_in_extent(const arena_extent_t *extent, const void *ptr) {
  const unsigned char *p = ptr;
  const unsigned char *start = extent->buddy.memory_start;
  const unsigned char *end = extent->buddy.memory_end;

  return p >= start && p < end;
}

void arena_free_slab_region(arena_t *arena, arena_extent_t *extent, void *ptr) {
  if (arena == NULL || extent == NULL || ptr == NULL) {
    return;
  }

#if SIGMA_DEBUG
  bool belongs_to_arena = false;

  for (arena_extent_t *it = arena->extents; it != NULL; it = it->next) {
    if (it == extent) {
      belongs_to_arena = true;
      break;
    }
  }

  if (!belongs_to_arena) {
    panic("arena_free_slab_region: extent does not belong to arena");
  }

  if (!pointer_in_extent(extent, ptr)) {
    panic("arena_free_slab_region: slab pointer outside extent");
  }
#endif

  buddy_free(&extent->buddy, ptr);
}

void arena_remote_free(arena_t *arena, void *ptr) {
  if (arena == NULL || ptr == NULL) {
    return;
  }

  remote_free_node_t *node = ptr;
  remote_free_node_t *head;

  do {
    head = atomic_load_explicit(&arena->remote_frees, memory_order_relaxed);

    node->next = head;
  } while (!atomic_compare_exchange_weak_explicit(&arena->remote_frees, &head,
                                                  node, memory_order_release,
                                                  memory_order_relaxed));

#if SIGMA_DEBUG
  atomic_fetch_add_explicit(&arena->remote_free_count, 1, memory_order_relaxed);
#endif
}

void arena_drain_remote_frees(arena_t *arena) {
  if (arena == NULL) {
    return;
  }

  remote_free_node_t *node = atomic_exchange_explicit(
      &arena->remote_frees, NULL, memory_order_acquire);

  while (node != NULL) {
    remote_free_node_t *next = node->next;
    alloc_header_t *alloc_header = alloc_header_from_user(node);

    if (alloc_header->type == ALLOC_TYPE_SLAB) {
      arena_free_local(arena, node);
    } else if (alloc_header->type == ALLOC_TYPE_BUDDY) {
      arena_free_buddy_local(arena, node);
    }

    node = next;
  }
}

void *arena_alloc(arena_t *arena, usize size) {
  if (arena == NULL || size == 0 || size > MAX_SLAB_OBJ_SIZE) {
    return NULL;
  }

  arena->alloc_counter++;

  if ((arena->alloc_counter & (ARENA_REMOTE_DRAIN_INTERVAL - 1)) == 0) {
    arena_drain_remote_frees(arena);
  }

  void *ptr = slab_alloc(arena, size);

  if (ptr != NULL) {
#if SIGMA_DEBUG
    arena->allocation_count++;
#endif
    return ptr;
  }

  /*
   * Reclaim cross-thread frees before growing the arena.
   */
  arena_drain_remote_frees(arena);

  ptr = slab_alloc(arena, size);

#if SIGMA_DEBUG
  if (ptr != NULL) {
    arena->allocation_count++;
  }
#endif

  return ptr;
}

void arena_free_local(arena_t *arena, void *ptr) {
  if (arena == NULL || ptr == NULL) {
    return;
  }

  slab_free_local(arena, ptr);

#if SIGMA_DEBUG
  arena->free_count++;
#endif
}

void arena_free_buddy_local(arena_t *arena, void *ptr) {
  if (arena == NULL || ptr == NULL) {
    return;
  }
  alloc_header_t *alloc_header = alloc_header_from_user(ptr);
  buddy_header_t *header =
      SIGMA_CONTAINER_OF(alloc_header, buddy_header_t, header);
  if (alloc_header->type != ALLOC_TYPE_BUDDY || header->arena != arena ||
      header->pool == NULL) {
    panic("arena_free_buddy_local: invalid pointer or wrong arena");
  }
  buddy_free(header->pool, ptr);
#if SIGMA_DEBUG
  arena->free_count++;
#endif
}
