#include "../include/libc_wrappers.h"
#include "../include/memory_source.h"

static void *source_alloc(void *ctx, usize size, usize alignment) {
  (void)ctx;
  (void)alignment;

  return sigma_libc_malloc(size);
}

static void source_free(void *ctx, void *ptr, usize size) {
  (void)ctx;
  (void)size;

  sigma_libc_free(ptr);
}

const memory_source_t malloc_memory_source = {
    .alloc = source_alloc,
    .free = source_free,
    .ctx = NULL,
    .returns_zeroed_memory = false,
};
