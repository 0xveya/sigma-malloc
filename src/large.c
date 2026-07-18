#include "../include/large.h"
#include "../include/arena.h"
#include "../include/qol.h"
#include "../include/sigma_malloc.h"
#include "../include/slab.h"

#include <stddef.h>
#if SIGMA_DEBUG
#include <pthread.h>
#endif
#include <sys/mman.h>
#include <unistd.h>

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

void *large_alloc(usize size) {
  usize total_size =
      size + offsetof(large_header_t, header) + sizeof(alloc_header_t);
  isize system_page_size = sysconf(_SC_PAGESIZE);
  if (system_page_size <= 0)
    return NULL;
  usize page_size = (usize)system_page_size;
  usize aligned_size = (total_size + page_size - 1) & ~(page_size - 1);

  void *mmap_ptr = mmap(NULL, aligned_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mmap_ptr == MAP_FAILED)
    return NULL;

  arena_t *arena = arena_get();
  if (arena == NULL) {
    munmap(mmap_ptr, aligned_size);
    return NULL;
  }
  large_node_t *node = (large_node_t *)arena_alloc(arena, sizeof(*node));
  large_metadata_t *meta =
      (large_metadata_t *)arena_alloc(arena, sizeof(*meta));

  if (!node || !meta) {
    if (node)
      cock(node);
    if (meta)
      cock(meta);
    munmap(mmap_ptr, aligned_size);
    return NULL;
  }

  node->mmap_ptr = mmap_ptr;
  node->mmap_size = aligned_size;

  meta->node = node;
  meta->alloc_file = NULL;
  meta->alloc_func = NULL;
  meta->alloc_line = 0;

  large_header_t *header = (large_header_t *)mmap_ptr;
  header->meta = meta;
  header->header.magic = LARGE_MAGIC;
  header->header.type = ALLOC_TYPE_LARGE;

#if SIGMA_DEBUG
  large_debug_list_lock();
  node->prev = NULL;
  node->next = g_alloc.large_allocs_head;
  if (g_alloc.large_allocs_head) {
    g_alloc.large_allocs_head->prev = node;
  }
  g_alloc.large_allocs_head = node;
  large_debug_list_unlock();
#endif

  return (void *)((u8 *)&header->header + sizeof(alloc_header_t));
}

void large_free(void *ptr) {
  if (!ptr)
    return;

  alloc_header_t *ah = alloc_header_from_user(ptr);
  large_header_t *header = SIGMA_CONTAINER_OF(ah, large_header_t, header);
  large_metadata_t *meta = header->meta;
  large_node_t *node = meta->node;

#if SIGMA_DEBUG
  large_debug_list_lock();
  if (node->prev) {
    node->prev->next = node->next;
  } else {
    g_alloc.large_allocs_head = node->next;
  }
  if (node->next) {
    node->next->prev = node->prev;
  }
  large_debug_list_unlock();
#endif

  void *mmap_ptr = node->mmap_ptr;
  usize mmap_size = node->mmap_size;

  cock(meta);
  cock(node);

  munmap(mmap_ptr, mmap_size);
}
