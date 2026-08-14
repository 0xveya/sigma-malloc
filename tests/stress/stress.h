#pragma once

#include "arena_allocator.h"
#include "sigma_malloc.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef void *(*alloc_fn)(size_t size, const char *file, const char *func,
                          int line);
typedef void *(*realloc_fn)(void *, size_t, size_t);
typedef void (*free_fn)(void *, size_t);
typedef void (*allocator_lifecycle_fn)(void);

typedef struct {
  alloc_fn allocate;
  realloc_fn reallocate;
  free_fn deallocate;
  allocator_lifecycle_fn prepare;
  allocator_lifecycle_fn cleanup;
  const char *name;
} allocator_api;

typedef enum {
  ALLOCATOR_SYSTEM,
  ALLOCATOR_CUSTOM,
  ALLOCATOR_CUSTOM_ARENA,
} AllocatorKind;

typedef enum {
  VERIFY_NONE,
  VERIFY_SAMPLE,
  VERIFY_FULL,
} VerifyMode;

typedef enum {
  OUTPUT_HUMAN,
  OUTPUT_JSON,
  OUTPUT_QUIET,
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

static void system_deallocate(void *ptr, size_t size) {
  (void)size;
  free(ptr);
}

static void *system_reallocate(void *ptr, size_t old_size, size_t new_size) {
  (void)old_size;
  return realloc(ptr, new_size);
}

static sigma_allocator_t stress_sigma;

static void stress_allocator_init(void) {
#ifdef SIGMA_MALLOC_BACKEND
  sigma_allocator_init(&stress_sigma, &malloc_memory_source);
#else
  sigma_allocator_init(&stress_sigma, &mmap_memory_source);
#endif
}

static allocator_t sigma_stress_allocator(void) {
  return sigma_allocator(&stress_sigma);
}

static void *sigma_allocate(size_t size, const char *file, const char *func,
                            int line) {
  (void)file;
  (void)func;
  (void)line;
  return allocator_malloc(sigma_stress_allocator(), size);
}

static void sigma_deallocate(void *ptr, size_t size) {
  allocator_free_sized(sigma_stress_allocator(), ptr, size);
}

static void *sigma_reallocate(void *ptr, size_t old_size, size_t new_size) {
  return allocator_realloc_sized(sigma_stress_allocator(), ptr, old_size,
                                 new_size);
}

static _Thread_local allocator_arena_t stress_arena;
static _Thread_local bool stress_arena_initialized;

static void stress_arena_prepare(void) {
  allocator_arena_init(&stress_arena, sigma_stress_allocator(), 64 * 1024);
  stress_arena_initialized = true;
}

static void *stress_arena_allocate(size_t size, const char *file,
                                   const char *func, int line) {
  (void)file;
  (void)func;
  (void)line;
  return allocator_malloc(allocator_arena(&stress_arena), size);
}

static void stress_arena_deallocate(void *ptr, size_t size) {
  allocator_free_sized(allocator_arena(&stress_arena), ptr, size);
}

static void *stress_arena_reallocate(void *ptr, size_t old_size,
                                     size_t new_size) {
  return allocator_realloc_sized(allocator_arena(&stress_arena), ptr, old_size,
                                 new_size);
}

static void stress_arena_cleanup(void) {
  if (stress_arena_initialized)
    allocator_arena_deinit(&stress_arena);
  stress_arena_initialized = false;
}

#define allocator_alloc(api, size)                                             \
  ((api)->allocate((size), __FILE__, __func__, __LINE__))

static const allocator_api system_allocator = {
    .allocate = system_allocate,
    .reallocate = system_reallocate,
    .deallocate = system_deallocate,
    .prepare = NULL,
    .cleanup = NULL,
    .name = "system",
};

static const allocator_api custom_allocator = {
    .allocate = sigma_allocate,
    .reallocate = sigma_reallocate,
    .deallocate = sigma_deallocate,
    .prepare = NULL,
    .cleanup = NULL,
    .name = "custom",
};

static const allocator_api custom_arena_allocator = {
    .allocate = stress_arena_allocate,
    .reallocate = stress_arena_reallocate,
    .deallocate = stress_arena_deallocate,
    .prepare = stress_arena_prepare,
    .cleanup = stress_arena_cleanup,
    .name = "custom-arena",
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
