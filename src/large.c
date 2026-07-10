#include "../include/large.h"
#include "../include/qol.h"
#include "../include/sigma_malloc.h"
#include "../include/slab.h"

#include <sys/mman.h>
#include <unistd.h>
void *large_alloc(usize size) {
  usize total_size = size + sizeof(large_header_t);
  usize page_size = sysconf(_SC_PAGESIZE);
  usize aligned_size = (total_size + page_size - 1) & ~(page_size - 1);

  void *mmap_ptr = mmap(NULL, aligned_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mmap_ptr == MAP_FAILED)
    return NULL;

  large_node_t *node = (large_node_t *)slab_alloc(sizeof(large_node_t));
  large_metadata_t *meta =
      (large_metadata_t *)slab_alloc(sizeof(large_metadata_t));

  if (!node || !meta) {
    if (node)
      slab_free(node);
    if (meta)
      slab_free(meta);
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

  node->prev = NULL;
  node->next = g_alloc.large_allocs_head;
  if (g_alloc.large_allocs_head) {
    g_alloc.large_allocs_head->prev = node;
  }
  g_alloc.large_allocs_head = node;

  return (void *)((u8 *)mmap_ptr + sizeof(large_header_t));
}

void large_free(void *ptr) {
  if (!ptr)
    return;

  large_header_t *header =
      (large_header_t *)((u8 *)ptr - sizeof(large_header_t));
  large_metadata_t *meta = header->meta;
  large_node_t *node = meta->node;

  if (node->prev) {
    node->prev->next = node->next;
  } else {
    g_alloc.large_allocs_head = node->next;
  }
  if (node->next) {
    node->next->prev = node->prev;
  }

  void *mmap_ptr = node->mmap_ptr;
  usize mmap_size = node->mmap_size;

  slab_free(meta);
  slab_free(node);

  munmap(mmap_ptr, mmap_size);
}
