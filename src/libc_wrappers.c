#include "../include/libc_wrappers.h"

void *sigma_libc_memset(void *ptr, int value, usize size) {
  u8 *bytes = ptr;
  for (usize index = 0; index < size; index++)
    bytes[index] = (u8)value;
  return ptr;
}

void *sigma_memcpy(void *destination, const void *source, usize size) {
  u8 *to = destination;
  const u8 *from = source;

  for (usize i = 0; i < size; i++)
    to[i] = from[i];
  return destination;
}
