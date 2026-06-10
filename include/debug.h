#pragma once

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
    obj_header_t *__hdr =                                                      \
        (obj_header_t *)((uint8_t *)(ptr) - sizeof(obj_header_t));             \
    __hdr->alloc_file = __FILE__;                                              \
    __hdr->alloc_func = __func__;                                              \
    __hdr->alloc_line = __LINE__;                                              \
  } while (0)
#else
#define balls_debug_tag(ptr) ((void)(ptr))
#endif
