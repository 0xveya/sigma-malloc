#include "../include/memory_source.h"
#include <sigma/sys.h>

static void *source_alloc(void *ctx, usize size, usize alignment) {
  (void)ctx;
  (void)alignment;

  sigma_mmap_result_t result =
      s_mmap(NULL, size, SIGMA_PROT_READ | SIGMA_PROT_WRITE,
             SIGMA_MAP_PRIVATE | SIGMA_MAP_ANONYMOUS, -1, 0);
  return result.ok ? result.value : NULL;
}

static void source_free(void *ctx, void *ptr, usize size) {
  (void)ctx;

  if (ptr)
    (void)s_munmap(ptr, size);
}

const memory_source_t mmap_memory_source = {
    .alloc = source_alloc,
    .free = source_free,
    .ctx = NULL,
    .returns_zeroed_memory = true,
};
