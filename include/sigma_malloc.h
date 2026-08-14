#pragma once

#include "allocator_vtable.h"
#include "debug.h"
#include "large.h"
#include "memory_source.h"
#include "qol.h"

/* Concrete sigma allocator state. Initialize before requesting a handle. */
typedef struct sigma_allocator {
  bool initialized;
  bool is_debug;

  const memory_source_t *source;

  large_node_t *large_allocs_head;
} sigma_allocator_t;

extern sigma_allocator_t g_alloc;
void *balls_backend(usize size);
void *balls_debug_backend(usize size, const char *file, const char *func,
                          i32 line);
void cock(void *ptr);

#if SIGMA_DEBUG
#define balls(size) balls_debug_backend((size), __FILE__, __func__, __LINE__)
#else
#define balls(size) balls_backend(size)
#endif

/* Return a generic handle borrowing sigma. */
allocator_t sigma_allocator(sigma_allocator_t *sigma);

/* Initialize a concrete instance with a backing memory source. */
void sigma_allocator_init(sigma_allocator_t *sigma,
                          const memory_source_t *source);

void *sigma_alloc(sigma_allocator_t *sigma, usize size, usize alignment);
void *sigma_alloc_debug(sigma_allocator_t *sigma, usize size, usize alignment,
                        const char *file, const char *func, i32 line);
/* Concrete operations used by the generic adapter. Prefer typed allocator_t
 * helpers in application code. */
void *sigma_calloc(sigma_allocator_t *sigma, usize count, usize size,
                   usize alignment);
void *sigma_realloc(sigma_allocator_t *sigma, void *ptr, usize old_size,
                    usize new_size, usize alignment);
void sigma_free(sigma_allocator_t *sigma, void *ptr);
