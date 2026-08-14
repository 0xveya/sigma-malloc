#include "./include/arena_allocator.h"
#include "./include/memory_source.h"
#include "./include/sigma_malloc.h"

#include <stdint.h>
#include <stdio.h>

typedef struct cache_line {
  _Alignas(64) unsigned char bytes[64];
} cache_line_t;

static bool test_allocator(const char *name, allocator_t allocator) {
  int *values = allocator_array(allocator, int, 10);
  cache_line_t *line = allocator_new(allocator, cache_line_t);
  unsigned char *zeroed = allocator_array_zeroed(allocator, unsigned char, 128);

  if (values == NULL || line == NULL || zeroed == NULL) {
    fprintf(stderr, "%s: allocation failed\n", name);
    allocator_free_array(allocator, zeroed, 128);
    allocator_delete(allocator, line);
    allocator_free_array(allocator, values, 10);
    return false;
  }

  if ((uintptr_t)line % _Alignof(cache_line_t) != 0) {
    fprintf(stderr, "%s: typed allocation is not aligned\n", name);
    allocator_free_array(allocator, zeroed, 128);
    allocator_delete(allocator, line);
    allocator_free_array(allocator, values, 10);
    return false;
  }

  for (usize i = 0; i < 10; i++)
    values[i] = (int)(i * 67);
  for (usize i = 0; i < 128; i++) {
    if (zeroed[i] != 0) {
      fprintf(stderr, "%s: calloc allocation was not zeroed\n", name);
      allocator_free_array(allocator, zeroed, 128);
      allocator_delete(allocator, line);
      allocator_free_array(allocator, values, 10);
      return false;
    }
  }

  allocator_free_array(allocator, zeroed, 128);
  allocator_delete(allocator, line);
  allocator_free_array(allocator, values, 10);
  printf("%s backend: typed, aligned, and zeroed allocations passed\n", name);
  return true;
}

int main(void) {
  sigma_allocator_t mmap_sigma;
  sigma_allocator_t malloc_sigma;

  sigma_allocator_init(&mmap_sigma, &mmap_memory_source);
  sigma_allocator_init(&malloc_sigma, &malloc_memory_source);

  allocator_t mmap_allocator = sigma_allocator(&mmap_sigma);
  allocator_t malloc_allocator = sigma_allocator(&malloc_sigma);
  allocator_arena_t mmap_arena;
  allocator_arena_t malloc_arena;

  allocator_arena_init(&mmap_arena, mmap_allocator, 16 * 1024);
  allocator_arena_init(&malloc_arena, malloc_allocator, 16 * 1024);

  bool passed = test_allocator("mmap arena", allocator_arena(&mmap_arena)) &&
                test_allocator("malloc arena", allocator_arena(&malloc_arena));

  allocator_arena_deinit(&malloc_arena);
  allocator_arena_deinit(&mmap_arena);

  if (!passed)
    return 67;
  return 0;
}
