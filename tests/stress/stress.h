#pragma once

#include "sigma_malloc.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef void *(*alloc_fn)(size_t size, const char *file, const char *func,
                          int line);
typedef void (*free_fn)(void *);

typedef struct {
  alloc_fn allocate;
  free_fn deallocate;
  const char *name;
} allocator_api;

typedef enum {
  ALLOCATOR_SYSTEM,
  ALLOCATOR_CUSTOM,
} AllocatorKind;

typedef enum {
  VERIFY_NONE,
  VERIFY_SAMPLE,
  VERIFY_FULL,
} VerifyMode;

typedef enum {
  OUTPUT_HUMAN,
  OUTPUT_JSON,
} OutputMode;

typedef struct {
  const allocator_api *allocator;
  unsigned threads;

  uint64_t target_bytes;
  uint64_t min_size;
  uint64_t max_size;
  uint64_t slots;
  uint64_t cycles;
  uint64_t seed;

  unsigned fragment_percent;
  unsigned stats_interval_ms;

  VerifyMode verify;
  OutputMode output;
  bool abort_on_error;
} Options;

static void *system_allocate(size_t size, const char *file, const char *func,
                             int line) {
  (void)file;
  (void)func;
  (void)line;
  return malloc(size);
}

static void *sigma_allocate(size_t size, const char *file, const char *func,
                            int line) {
#if SIGMA_DEBUG
  return balls_debug_backend(size, file, func, line);
#else
  (void)file;
  (void)func;
  (void)line;
  return balls_backend(size);
#endif
}

#define allocator_alloc(api, size)                                             \
  ((api)->allocate((size), __FILE__, __func__, __LINE__))

static const allocator_api system_allocator = {
    .allocate = system_allocate,
    .deallocate = free,
    .name = "system",
};

static const allocator_api custom_allocator = {
    .allocate = sigma_allocate,
    .deallocate = cock,
    .name = "custom",
};

static Options options_default(void) {
  return (Options){
      .allocator = &custom_allocator,
      .threads = 1,
      .target_bytes = 16ULL * 1024 * 1024,
      .min_size = 1,
      .max_size = 64ULL * 1024,
      .slots = 4096,
      .cycles = 2,
      .seed = 12345,
      .fragment_percent = 50,
      .stats_interval_ms = 1000,
      .verify = VERIFY_FULL,
      .output = OUTPUT_HUMAN,
      .abort_on_error = true,
  };
}

int run_stress_test(const Options *options);

typedef struct {
  void *ptr;
  size_t size;
  uint64_t allocation_id;
  uint64_t pattern_seed;
} allocation_slot;

typedef struct {
  const Options *options;

  unsigned thread_index;
  uint64_t rng_state;
  uint64_t next_allocation_id;

  allocation_slot *slots;
  size_t slot_count;

  uint64_t live_bytes;
  uint64_t peak_live_bytes;
  uint64_t live_allocations;
  uint64_t allocation_count;
  uint64_t free_count;
  uint64_t verification_count;
  uint64_t allocation_failures;
} worker_state;
