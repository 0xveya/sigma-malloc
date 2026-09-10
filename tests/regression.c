#include "../tests_c.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                       \
  do {                                                                         \
    if (!(condition)) {                                                        \
      fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__,       \
              #condition);                                                     \
      return false;                                                            \
    }                                                                          \
  } while (0)

typedef bool (*test_fn)(void);
typedef struct {
  const char *name;
  test_fn run;
} test_case_t;
typedef struct {
  void *ptr;
  usize size;
} slot_t;

static void fill(void *ptr, usize size, u8 seed) {
  volatile u8 *bytes = ptr;
  for (usize index = 0; index < size; ++index)
    bytes[index] = (u8)(seed + (u8)index);
}

static void fill_edges(void *ptr, usize size, u8 tag) {
  volatile u8 *bytes = ptr;
  bytes[0] = tag;
  bytes[size - 1] = tag ^ 0xa5;
  if (size > 2)
    bytes[size / 2] = tag ^ 0x5a;
}

static void *alloc_checked(usize size) {
  void *ptr = sigma_alloc(&sigma_test_allocator, size, alignof(usize));
  if (ptr == NULL || (uintptr_t)ptr % alignof(usize) != 0)
    return NULL;
  return ptr;
}

static bool slab_classes(void) {
  static const usize sizes[] = {1,   16,  17,  32,  33,  64,  65,  128,
                                129, 256, 257, 512, 513, 768, 769, 1024};
  for (usize index = 0; index < sizeof(sizes) / sizeof(*sizes); ++index) {
    void *ptr = alloc_checked(sizes[index]);
    CHECK(ptr != NULL);
    fill(ptr, sizes[index], (u8)sizes[index]);
    sigma_free(&sigma_test_allocator, ptr);
  }
  return true;
}

static bool slab_reuse(void) {
  static const usize sizes[] = {1,   16,  17,  32,  33,  64,  65,  128,
                                129, 256, 257, 512, 513, 768, 769, 1024};
  for (usize size_index = 0; size_index < sizeof(sizes) / sizeof(*sizes);
       ++size_index) {
    void *ptrs[48];
    for (usize index = 0; index < 48; ++index) {
      ptrs[index] = alloc_checked(sizes[size_index]);
      CHECK(ptrs[index] != NULL);
      fill(ptrs[index], sizes[size_index], (u8)index);
    }
    for (usize index = 0; index < 48; index += 2)
      sigma_free(&sigma_test_allocator, ptrs[index]);
    for (usize index = 0; index < 48; index += 2) {
      ptrs[index] = alloc_checked(sizes[size_index]);
      CHECK(ptrs[index] != NULL);
    }
    for (usize index = 0; index < 48; ++index)
      sigma_free(&sigma_test_allocator, ptrs[index]);
  }
  return true;
}

static bool slab_boundaries(void) {
  static const usize sizes[] = {15,  16,  17,  31,  32,  33,   63,  64,
                                65,  127, 128, 129, 255, 256,  257, 511,
                                512, 513, 767, 768, 769, 1023, 1024};
  void *ptrs[sizeof(sizes) / sizeof(*sizes)];
  for (usize pass = 0; pass < 2; ++pass) {
    for (usize index = 0; index < sizeof(sizes) / sizeof(*sizes); ++index) {
      ptrs[index] = alloc_checked(sizes[index]);
      CHECK(ptrs[index] != NULL);
      fill(ptrs[index], sizes[index], (u8)(index + pass));
    }
    for (usize index = 0; index < sizeof(ptrs) / sizeof(*ptrs); ++index)
      sigma_free(&sigma_test_allocator, ptrs[index]);
  }
  return true;
}

typedef struct {
  void **ptrs;
  usize len;
} free_args_t;
static void *free_thread(void *opaque) {
  free_args_t *args = opaque;
  for (usize index = 0; index < args->len; ++index)
    sigma_free(&sigma_test_allocator, args->ptrs[index]);
  return NULL;
}

static bool cross_thread(usize size, usize count) {
  void *ptrs[128];
  pthread_t threads[4];
  free_args_t args[4];
  for (usize index = 0; index < count; ++index) {
    ptrs[index] = alloc_checked(size);
    CHECK(ptrs[index] != NULL);
  }
  for (usize index = 0; index < 4; ++index) {
    args[index] =
        (free_args_t){.ptrs = ptrs + index * (count / 4), .len = count / 4};
    CHECK(pthread_create(&threads[index], NULL, free_thread, &args[index]) ==
          0);
  }
  for (usize index = 0; index < 4; ++index)
    CHECK(pthread_join(threads[index], NULL) == 0);
  for (usize index = 0; index < 64; ++index) {
    void *ptr = alloc_checked(size);
    CHECK(ptr != NULL);
    fill(ptr, size, 0xa5);
    sigma_free(&sigma_test_allocator, ptr);
  }
  return true;
}

static bool slab_remote(void) { return cross_thread(64, 128); }
static bool buddy_remote(void) { return cross_thread(4096, 64); }

static bool buddy_boundaries(void) {
  static const usize sizes[] = {1025,  2048,  4096,   8192,   16384,
                                32768, 65536, 131072, 262144, 524288};
  for (usize index = 0; index < sizeof(sizes) / sizeof(*sizes); ++index) {
    void *ptr = alloc_checked(sizes[index]);
    CHECK(ptr != NULL);
    fill(ptr, sizes[index], (u8)(sizes[index] >> 8));
    sigma_free(&sigma_test_allocator, ptr);
  }
  return true;
}

static bool buddy_reuse(void) {
  void *ptrs[128];
  for (usize pass = 0; pass < 2; ++pass) {
    for (usize index = 0; index < 128; ++index) {
      ptrs[index] = alloc_checked(4096);
      CHECK(ptrs[index] != NULL);
      fill(ptrs[index], 4096, (u8)(index + pass));
    }
    for (usize index = 0; index < 128; ++index)
      sigma_free(&sigma_test_allocator, ptrs[index]);
  }
  return true;
}

static bool large_allocations(void) {
  static const usize sizes[] = {4 * 1024 * 1024, 4 * 1024 * 1024 + 1,
                                6 * 1024 * 1024};
  for (usize index = 0; index < 3; ++index) {
    void *ptr = alloc_checked(sizes[index]);
    CHECK(ptr != NULL);
    fill(ptr, sizes[index], (u8)(sizes[index] >> 12));
    sigma_free(&sigma_test_allocator, ptr);
  }
  return true;
}

static bool debug_leaks(void) {
  if (!sigma_debug_enabled())
    return true;
  sigma_debug_reset_leaks();
  void *ptrs[] = {alloc_checked(64), alloc_checked(4096),
                  alloc_checked(4 * 1024 * 1024 + 128)};
  CHECK(ptrs[0] && ptrs[1] && ptrs[2]);
  CHECK(sigma_debug_collect_leaks(&sigma_test_allocator) == 3);
  for (usize index = 0; index < 3; ++index)
    sigma_free(&sigma_test_allocator, ptrs[index]);
  CHECK(sigma_debug_collect_leaks(&sigma_test_allocator) == 0);
  sigma_debug_reset_leaks();
  return true;
}

static void *large_thread(void *slot) {
  *(void **)slot =
      sigma_alloc(&sigma_test_allocator, 4 * 1024 * 1024 + 1, alignof(usize));
  return NULL;
}

static bool debug_concurrent(void) {
  if (!sigma_debug_enabled())
    return true;
  void *ptrs[8] = {0};
  pthread_t threads[8];
  sigma_debug_reset_leaks();
  for (usize i = 0; i < 8; ++i)
    CHECK(pthread_create(&threads[i], NULL, large_thread, &ptrs[i]) == 0);
  for (usize i = 0; i < 8; ++i)
    CHECK(pthread_join(threads[i], NULL) == 0 && ptrs[i] != NULL);
  CHECK(sigma_debug_collect_leaks(&sigma_test_allocator) == 8);
  for (usize i = 0; i < 8; ++i)
    sigma_free(&sigma_test_allocator, ptrs[i]);
  CHECK(sigma_debug_collect_leaks(&sigma_test_allocator) == 0);
  return true;
}

static uint64_t random_state;
static uint64_t next_random(void) {
  random_state ^= random_state << 13;
  random_state ^= random_state >> 7;
  random_state ^= random_state << 17;
  return random_state;
}

static bool churn(void) {
  slot_t slots[64] = {0};
  random_state = UINT64_C(0x51a6a110c);
  for (usize op = 0; op < 8000; ++op) {
    usize index = next_random() % 64;
    if (slots[index].ptr) {
      sigma_free(&sigma_test_allocator, slots[index].ptr);
      slots[index] = (slot_t){0};
      continue;
    }
    usize size = next_random() % 10 < 7 ? 1 + next_random() % 1024
                                        : 1025 + next_random() % (32 * 1024);
    slots[index] = (slot_t){.ptr = alloc_checked(size), .size = size};
    CHECK(slots[index].ptr != NULL);
    fill(slots[index].ptr, size, (u8)index);
  }
  for (usize i = 0; i < 64; ++i)
    if (slots[i].ptr)
      sigma_free(&sigma_test_allocator, slots[i].ptr);
  return true;
}

static bool fuzz_sizes(void) {
  random_state = UINT64_C(0xf00dcafe5151);
  for (usize i = 0; i < 1500; ++i) {
    usize size = 1 + next_random() % (256 * 1024);
    void *ptr = alloc_checked(size);
    CHECK(ptr != NULL);
    fill(ptr, size, (u8)size);
    sigma_free(&sigma_test_allocator, ptr);
  }
  return true;
}

static bool fragmentation(void) {
  slot_t slots[96] = {0};
  for (usize i = 0; i < 96; ++i) {
    usize size = i % 3 == 0 ? 32 + i % 17 * 13 : 1025 + i * 251;
    slots[i] = (slot_t){alloc_checked(size), size};
    CHECK(slots[i].ptr);
  }
  for (usize i = 0; i < 96; i += 2) {
    sigma_free(&sigma_test_allocator, slots[i].ptr);
    usize size = i % 4 == 0 ? 64 + i * 3 : 2048 + i * 97;
    slots[i] = (slot_t){alloc_checked(size), size};
    CHECK(slots[i].ptr);
  }
  for (usize i = 0; i < 96; ++i)
    sigma_free(&sigma_test_allocator, slots[i].ptr);
  return true;
}

static bool generic_allocator(void) {
  allocator_t allocator = sigma_allocator(&sigma_test_allocator);
  static const usize alignments[] = {16, 64, 4096};
  for (usize i = 0; i < 3; ++i) {
    void *ptr = allocator_alloc_aligned(allocator, 8192, alignments[i]);
    CHECK(ptr && (uintptr_t)ptr % alignments[i] == 0);
    allocator_free_aligned(allocator, ptr, 8192, alignments[i]);
  }
  void *ptr = sigma_calloc(&sigma_test_allocator, 256, 1, 64);
  CHECK(ptr);
  for (usize i = 0; i < 256; ++i)
    CHECK(((u8 *)ptr)[i] == 0);
  ptr = sigma_realloc(&sigma_test_allocator, ptr, 256, 1024, 64);
  CHECK(ptr && (uintptr_t)ptr % 64 == 0);
  sigma_free(&sigma_test_allocator, ptr);
  return true;
}

static bool arena_allocator(void) {
  allocator_arena_t arena;
  allocator_arena_init(&arena, sigma_allocator(&sigma_test_allocator), 4096);
  allocator_t allocator = allocator_arena(&arena);
  void *first = allocator_alloc_aligned(allocator, 100, 64);
  void *second = allocator_alloc_aligned(allocator, 9000, 4096);
  CHECK(first && second && (uintptr_t)first % 64 == 0 &&
        (uintptr_t)second % 4096 == 0);
  first = allocator_realloc_aligned(allocator, first, 100, 300, 64);
  CHECK(first && (uintptr_t)first % 64 == 0);
  allocator_arena_reset(&arena);
  CHECK(arena.blocks == NULL);
  allocator_arena_deinit(&arena);
  return true;
}

static bool debug_realloc_source(void) {
  if (!sigma_debug_enabled())
    return true;
  void *ptr = sigma_alloc_debug(&sigma_test_allocator, 32, alignof(usize),
                                "debug-origin.c", "debug_origin", 4242);
  CHECK(ptr);
  ptr = sigma_realloc(&sigma_test_allocator, ptr, 32, 4096, 64);
  CHECK(ptr);
  LeakInfo info;
  CHECK(sigma_debug_allocation_info(ptr, &info));
  CHECK(info.line == 4242 && strcmp(info.file, "debug-origin.c") == 0 &&
        strcmp(info.func, "debug_origin") == 0 && info.size == 4096);
  sigma_free(&sigma_test_allocator, ptr);
  return true;
}

static bool debug_arena_source(void) {
  if (!sigma_debug_enabled())
    return true;
  sigma_allocator_t sigma;
  sigma_allocator_init(&sigma, &mmap_memory_source);
  allocator_arena_t arena;
  allocator_arena_init(&arena, sigma_allocator(&sigma), 4096);
  void *ptr =
      allocator_alloc_aligned_debug(allocator_arena(&arena), 64, alignof(usize),
                                    "arena-origin.c", "arena_origin", 6767);
  CHECK(ptr);
  CHECK(sigma_debug_collect_leaks(&sigma) == 1);
  LeakInfo info;
  CHECK(sigma_debug_leak_info(0, &info));
  CHECK(info.line == 6767 && strcmp(info.file, "arena-origin.c") == 0 &&
        strcmp(info.func, "arena_origin") == 0);
  allocator_arena_deinit(&arena);
  return true;
}

static usize fuzz_size(void) {
  static const usize bases[] = {1,   17,  33,   65,        129,
                                257, 513, 1025, 16 * 1024, 4 * 1024 * 1024};
  static const usize spans[] = {16,  16,  32,   64,    128,
                                256, 512, 8192, 65536, 4096};
  usize bucket = next_random() % 10;
  return bases[bucket] + next_random() % spans[bucket];
}

static bool fuzz_single(void) {
  random_state = UINT64_C(0x713deed);
  for (usize i = 0; i < 512; ++i) {
    usize size = fuzz_size(), alignment = (usize)1 << (3 + next_random() % 10);
    void *ptr = sigma_alloc(&sigma_test_allocator, size, alignment);
    if (!ptr)
      continue;
    CHECK((uintptr_t)ptr % alignment == 0);
    fill_edges(ptr, size, (u8)next_random());
    sigma_free(&sigma_test_allocator, ptr);
  }
  return true;
}

static bool fuzz_operations(void) {
  slot_t slots[32] = {0};
  random_state = UINT64_C(0x713feed);
  for (usize i = 0; i < 4096; ++i) {
    usize index = next_random() % 32, op = next_random() % 4;
    if (op == 0 && slots[index].ptr) {
      sigma_free(&sigma_test_allocator, slots[index].ptr);
      slots[index] = (slot_t){0};
    } else if (op == 1 && slots[index].ptr)
      fill_edges(slots[index].ptr, slots[index].size, (u8)next_random());
    else if (op >= 2) {
      if (slots[index].ptr)
        sigma_free(&sigma_test_allocator, slots[index].ptr);
      usize size = fuzz_size();
      void *ptr = sigma_alloc(&sigma_test_allocator, size, alignof(usize));
      if (ptr) {
        fill_edges(ptr, size, (u8)next_random());
        slots[index] = (slot_t){ptr, size};
      } else
        slots[index] = (slot_t){0};
    }
  }
  for (usize i = 0; i < 32; ++i)
    if (slots[i].ptr)
      sigma_free(&sigma_test_allocator, slots[i].ptr);
  return true;
}

static const test_case_t tests[] = {
    {"slab: every size class allocates writes and frees", slab_classes},
    {"slab: freed slots can be reused in every size class", slab_reuse},
    {"slab: boundary sizes stay reusable", slab_boundaries},
    {"slab: concurrent cross-thread frees are reclaimed by the owning arena",
     slab_remote},
    {"buddy: concurrent cross-thread frees are reclaimed by the owning arena",
     buddy_remote},
    {"buddy: boundary sizes allocate write and free", buddy_boundaries},
    {"buddy: pool recovers after many same-order frees", buddy_reuse},
    {"large: mmap allocations allocate write and free", large_allocations},
    {"debug: leak collector finds slab buddy and large leaks", debug_leaks},
    {"debug: concurrent large allocations keep leak tracking consistent",
     debug_concurrent},
    {"mixed fragmentation: slab and buddy holes can be refilled",
     fragmentation},
    {"allocator: deterministic randomized churn", churn},
    {"allocator: deterministic fuzz-sized single allocation cases", fuzz_sizes},
    {"generic allocator: alignment calloc and realloc", generic_allocator},
    {"arena allocator: composes over sigma and releases as one lifetime",
     arena_allocator},
    {"debug: allocation source survives realloc", debug_realloc_source},
    {"debug: arena forwards allocation source to its parent",
     debug_arena_source},
    {"fuzz: allocator single allocation sizes", fuzz_single},
    {"fuzz: allocator single threaded operation sequences", fuzz_operations},
};

int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  for (usize index = 0; index < sizeof(tests) / sizeof(*tests); ++index)
    if (strcmp(argv[1], tests[index].name) == 0)
      return tests[index].run() ? 0 : 1;
  fprintf(stderr, "unknown test: %s\n", argv[1]);
  return 2;
}
