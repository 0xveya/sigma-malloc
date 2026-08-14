#include "sigma_malloc.h"

sigma_allocator_t sigma_test_allocator;

[[gnu::constructor]] static void sigma_test_allocator_init(void) {
#ifdef SIGMA_MALLOC_BACKEND
  sigma_allocator_init(&sigma_test_allocator, &malloc_memory_source);
#else
  sigma_allocator_init(&sigma_test_allocator, &mmap_memory_source);
#endif
}
