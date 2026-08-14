#include "../include/large.h"
#include "../include/arena.h"
#include "../include/qol.h"
#include "../include/sigma_malloc.h"
#include "../include/slab.h"
#include "../include/utils.h"

#include <stddef.h>
#if SIGMA_DEBUG
#include <pthread.h>
#endif

#if SIGMA_DEBUG
static pthread_mutex_t g_large_debug_list_mutex = PTHREAD_MUTEX_INITIALIZER;

void large_debug_list_lock(void) {
  (void)pthread_mutex_lock(&g_large_debug_list_mutex);
}

void large_debug_list_unlock(void) {
  (void)pthread_mutex_unlock(&g_large_debug_list_mutex);
}
#else
void large_debug_list_lock(void) {}
void large_debug_list_unlock(void) {}
#endif

void *large_alloc(sigma_allocator_t *allocator, usize size, usize alignment) {
  if (allocator == NULL || allocator->source == NULL || size == 0 ||
      !sigma_alignment_is_valid(alignment) ||
      size > SIZE_MAX - sizeof(large_header_t) - (alignment - 1))
    return NULL;

  usize total_size = size + sizeof(large_header_t) + alignment - 1;
  usize page_size = PAGE_SIZE;
  usize aligned_size = (total_size + page_size - 1) & ~(page_size - 1);

  void *backing =
      allocator->source->alloc(allocator->source->ctx, aligned_size, page_size);

  if (backing == NULL)
    return NULL;

  arena_t *arena = arena_get(allocator);
  if (arena == NULL) {
    allocator->source->free(allocator->source->ctx, backing, aligned_size);
    return NULL;
  }

  large_node_t *node =
      (large_node_t *)arena_alloc(arena, sizeof(*node), _Alignof(large_node_t));

  large_metadata_t *meta = (large_metadata_t *)arena_alloc(
      arena, sizeof(*meta), _Alignof(large_metadata_t));

  if (!node || !meta) {
    if (node)
      sigma_free(allocator, node);

    if (meta)
      sigma_free(allocator, meta);

    allocator->source->free(allocator->source->ctx, backing, aligned_size);

    return NULL;
  }

  node->backing = backing;
  node->backing_size = aligned_size;
  node->allocator = allocator;

  meta->node = node;
  meta->alloc_file = NULL;
  meta->alloc_func = NULL;
  meta->alloc_line = 0;

  uptr user_address =
      ALIGN_UP((uptr)backing + sizeof(large_header_t), alignment);
  large_header_t *header =
      (large_header_t *)(user_address - sizeof(large_header_t));
  node->user = (void *)user_address;

  header->meta = meta;
  header->header.magic = LARGE_MAGIC;
  header->header.type = ALLOC_TYPE_LARGE;
  header->header.requested_size = size;
  header->header.alignment = alignment;

#if SIGMA_DEBUG
  large_debug_list_lock();

  node->prev = NULL;
  node->next = allocator->large_allocs_head;

  if (allocator->large_allocs_head) {
    allocator->large_allocs_head->prev = node;
  }

  allocator->large_allocs_head = node;

  large_debug_list_unlock();
#endif

  return (void *)user_address;
}
void large_free(sigma_allocator_t *allocator, void *ptr) {
  if (!ptr)
    return;

  alloc_header_t *ah = alloc_header_from_user(ptr);

  large_header_t *header = SIGMA_CONTAINER_OF(ah, large_header_t, header);

  large_metadata_t *meta = header->meta;
  large_node_t *node = meta->node;

  if (node->allocator != allocator)
    panic("large_free: allocation belongs to another allocator");

#if SIGMA_DEBUG
  large_debug_list_lock();

  if (node->prev) {
    node->prev->next = node->next;
  } else {
    allocator->large_allocs_head = node->next;
  }

  if (node->next) {
    node->next->prev = node->prev;
  }

  large_debug_list_unlock();
#endif

  /*
   * Save these BEFORE freeing node, because node itself lives
   * inside sigma's arena.
   */
  void *backing = node->backing;
  usize backing_size = node->backing_size;

  sigma_free(allocator, meta);
  sigma_free(allocator, node);

  allocator->source->free(allocator->source->ctx, backing, backing_size);
}
