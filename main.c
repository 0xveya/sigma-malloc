#include "./include/memory_source.h"
#include "./include/sigma_malloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define var auto

static void test_allocator(const char *name, const memory_source_t *source) {
  printf("\n=== testing %s backend ===\n", name);

  g_alloc.source = source;

  var lol = (int *)balls(sizeof(int) * 10);
  if (!lol) {
    fprintf(stderr, "%s: failed small allocation\n", name);
    exit(67);
  }

  for (int i = 0; i < 10; i++) {
    lol[i] = i * 67;
    printf("%d\n", lol[i]);
  }

  var test = (char *)balls(1024 * 1024);
  if (!test) {
    fprintf(stderr, "%s: failed 1 MiB allocation\n", name);
    cock(lol);
    exit(67);
  }

  memset(test, 'f', 1024 * 1024);
  printf("test: %.10s\n", test);

  var test1 = (char *)balls(1024 * 10024);
  if (!test1) {
    fprintf(stderr, "%s: failed large allocation\n", name);
    cock(test);
    cock(lol);
    exit(67);
  }

  memset(test1, 'g', 1024 * 10024);
  printf("test1: %.10s\n", test1);

  cock(test1);
  cock(test);
  cock(lol);

  printf("=== %s backend passed ===\n", name);
}

int main(void) {
  test_allocator("mmap", &mmap_memory_source);
  test_allocator("malloc", &malloc_memory_source);

  return 0;
}
