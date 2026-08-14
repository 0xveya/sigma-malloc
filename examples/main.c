#include "arena_allocator.h"
#include "memory_source.h"
#include "parser.h"
#include "sigma_malloc.h"

#include <stdio.h>

#ifndef PARSER_EXAMPLE_FREE
#define PARSER_EXAMPLE_FREE 1
#endif

int main(void) {
  /* Pick a concrete backend, then erase its type behind allocator_t. */
  sigma_allocator_t sigma;
  sigma_allocator_init(&sigma, &mmap_memory_source);
  allocator_t heap = sigma_allocator(&sigma);

  /* Compose a short-lived arena over the generic parent allocator. */
  allocator_arena_t arena;
  allocator_arena_init(&arena, heap, 4096);
  allocator_t temporary = allocator_arena(&arena);

  /* Typed helpers infer size and alignment. The parser stores the allocator
   * used for every string and for its reallocating entry table. */
  parser_t *parser = allocator_new(heap, parser_t);
  if (parser == NULL) {
    allocator_arena_deinit(&arena);
    return 1;
  }
  parser_init(parser, temporary,
              "project=sigma_malloc; backend=mmap; workers=4");

  bool parsed = parser_parse(parser);
  if (parsed) {
    /* Zeroed typed arrays use the allocator's calloc operation. */
    usize *visits =
        allocator_array_zeroed(temporary, usize, parser->entry_count);
    if (visits == NULL) {
      parsed = false;
    } else {
      visits[0]++;
      printf("project=%s backend=%s workers=%s visits=%zu\n",
             parser_get(parser, "project"), parser_get(parser, "backend"),
             parser_get(parser, "workers"), visits[0]);
    }
  }

#if PARSER_EXAMPLE_FREE
  /* One deinit releases all parser strings, entries, and temporary arrays. */
  allocator_arena_deinit(&arena);
#else
  printf("PARSER_EXAMPLE_FREE=0: intentionally leaking parser arena\n");
#endif

  /* Heap-owned values still use their matching typed free helper. */
  allocator_delete(heap, parser);

  /* Debug reporting is explicit because allocator instances are explicit. */
  sigma_debug_report_leaks(&sigma);
  return parsed ? 0 : 1;
}
