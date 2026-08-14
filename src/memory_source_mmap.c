#include "../include/libc_wrappers.h"
#include "../include/memory_source.h"

static void *source_alloc(void *ctx, usize size, usize alignment) {
  (void)ctx;
  (void)alignment;

  return sigma_libc_map(size);
}

static void source_free(void *ctx, void *ptr, usize size) {
  (void)ctx;

  if (ptr)
    sigma_libc_unmap(ptr, size);
}

const memory_source_t mmap_memory_source = {
    .alloc = source_alloc,
    .free = source_free,
    .ctx = NULL,
    .returns_zeroed_memory = true,
};
