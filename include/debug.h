#pragma once

#include "qol.h"

typedef struct sigma_allocator sigma_allocator_t;

void sigma_debug_forget_freed(void *ptr);
bool sigma_debug_check_double_free(void *ptr);
void sigma_debug_record_free(void *ptr, const char *file, const char *func,
                             i32 line);

#ifndef USE_DEBUG_ALLOC
#define USE_DEBUG_ALLOC 0
#endif

#ifndef NO_LEAK_REWARD
#define NO_LEAK_REWARD 0
#endif

#ifndef HORNY_MODE
#define HORNY_MODE 0
#endif

#if USE_DEBUG_ALLOC == 1 || (USE_DEBUG_ALLOC == 0 && !defined(__OPTIMIZE__))
#define SIGMA_DEBUG 1
#else
#define SIGMA_DEBUG 0
#endif

typedef struct {
  const char *file;
  const char *func;
  i32 line;
  usize size;
  char type[64];
} LeakInfo;

/* Read retained allocation metadata without freeing ptr. Debug builds only. */
bool sigma_debug_allocation_info(void *ptr, LeakInfo *out);

usize sigma_debug_collect_leaks(sigma_allocator_t *sigma);
bool sigma_debug_leak_info(usize index, LeakInfo *out);
void sigma_debug_report_leaks(sigma_allocator_t *sigma);
int sigma_debug_enabled(void);
usize sigma_debug_leak_count(void);
void sigma_debug_reset_leaks(void);
