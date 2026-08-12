#include "../include/memory_source.h"

#include <sys/mman.h>

static void *source_alloc(void *ctx, usize size, usize alignment) {
  (void)ctx;
  (void)alignment;

  void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  return ptr == MAP_FAILED ? NULL : ptr;
}

static void source_free(void *ctx, void *ptr, usize size) {
  (void)ctx;

  if (ptr)
    munmap(ptr, size);
}

const memory_source_t mmap_memory_source = {
    .alloc = source_alloc,
    .free = source_free,
    .ctx = NULL,
};
