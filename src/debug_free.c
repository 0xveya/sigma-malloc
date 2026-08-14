#include "../include/debug.h"
#include "../include/utils.h"

#if SIGMA_DEBUG
#include <pthread.h>
#include <stdio.h>

#define SIGMA_FREED_RECORDS 8192

typedef struct freed_record {
  void *ptr;
  const char *file;
  const char *func;
  i32 line;
} freed_record_t;

static freed_record_t g_freed[SIGMA_FREED_RECORDS];
static usize g_freed_next;
static pthread_mutex_t g_freed_mutex = PTHREAD_MUTEX_INITIALIZER;

void sigma_debug_forget_freed(void *ptr) {
  (void)pthread_mutex_lock(&g_freed_mutex);
  for (usize i = 0; i < SIGMA_FREED_RECORDS; i++) {
    if (g_freed[i].ptr == ptr)
      g_freed[i] = (freed_record_t){0};
  }
  (void)pthread_mutex_unlock(&g_freed_mutex);
}

bool sigma_debug_check_double_free(void *ptr) {
  freed_record_t found = {0};

  (void)pthread_mutex_lock(&g_freed_mutex);
  for (usize i = 0; i < SIGMA_FREED_RECORDS; i++) {
    if (g_freed[i].ptr == ptr) {
      found = g_freed[i];
      break;
    }
  }
  (void)pthread_mutex_unlock(&g_freed_mutex);

  if (found.ptr == NULL)
    return false;

  if (found.file != NULL) {
    fprintf(stderr, "double free of %p; allocated at %s:%d in %s\n", ptr,
            found.file, found.line, found.func == NULL ? "?" : found.func);
  } else {
    fprintf(stderr, "double free of %p; allocation site unavailable\n", ptr);
  }
  panic("sigma_free: double free");
  return true;
}

void sigma_debug_record_free(void *ptr, const char *file, const char *func,
                             i32 line) {
  (void)pthread_mutex_lock(&g_freed_mutex);
  g_freed[g_freed_next] = (freed_record_t){
      .ptr = ptr,
      .file = file,
      .func = func,
      .line = line,
  };
  g_freed_next = (g_freed_next + 1) % SIGMA_FREED_RECORDS;
  (void)pthread_mutex_unlock(&g_freed_mutex);
}
#else
void sigma_debug_forget_freed(void *ptr) { (void)ptr; }
bool sigma_debug_check_double_free(void *ptr) {
  (void)ptr;
  return false;
}
void sigma_debug_record_free(void *ptr, const char *file, const char *func,
                             i32 line) {
  (void)ptr;
  (void)file;
  (void)func;
  (void)line;
}
#endif
