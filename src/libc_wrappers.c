#include "../include/libc_wrappers.h"

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

void *sigma_libc_malloc(usize size) { return malloc(size); }

void sigma_libc_free(void *ptr) { free(ptr); }

void *sigma_libc_map(usize size) {
  void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return ptr == MAP_FAILED ? NULL : ptr;
}

void sigma_libc_unmap(void *ptr, usize size) {
  if (ptr != NULL)
    (void)munmap(ptr, size);
}

void *sigma_libc_memset(void *ptr, int value, usize size) {
  return memset(ptr, value, size);
}

void *sigma_memcpy(void *destination, const void *source, usize size) {
#ifdef SIGMA_USE_LIBC_MEMCPY
  return memcpy(destination, source, size);
#else
  u8 *to = destination;
  const u8 *from = source;

  for (usize i = 0; i < size; i++)
    to[i] = from[i];
  return destination;
#endif
}
