#pragma once

#include "qol.h"

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

#if SIGMA_DEBUG
#define balls_debug_tag(ptr)                                                   \
  do {                                                                         \
    alloc_header_t *__ah = alloc_header_from_user(ptr);                        \
    obj_header_t *__hdr = SIGMA_CONTAINER_OF(__ah, obj_header_t, header);      \
    __hdr->alloc_file = __FILE__;                                              \
    __hdr->alloc_func = __func__;                                              \
    __hdr->alloc_line = __LINE__;                                              \
  } while (0)
#else
#define balls_debug_tag(ptr) ((void)(ptr))
#endif

typedef enum {
  RESULT_OK,
  RESULT_ERR,
} ResultStatus;

typedef struct {
  const char *file;
  const char *func;
  i32 line;
  usize size;
  char type[64];
  char source_line[1024];
} LeakInfo;

typedef struct {
  const char *message;
} LeakError;

typedef struct {
  ResultStatus status;
  union {
    LeakInfo ok;
    LeakError err;
  } value;
} LeakResult;

#define MAX_LEAKS 4096
static LeakResult g_leaks[MAX_LEAKS];
static usize g_leak_count = 0;

usize sigma_debug_collect_leaks(void);
int sigma_debug_enabled(void);
usize sigma_debug_leak_count(void);
void sigma_debug_reset_leaks(void);
