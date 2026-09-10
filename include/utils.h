#pragma once

#include "qol.h"
#include <sigma/sys.h>
#define ANSI_RED "\x1b[31m"
#define ANSI_DIM "\x1b[2m"
#define ANSI_BOLD "\x1b[1m"
#define ANSI_RESET "\x1b[0m"

typedef enum { READ_SUCCESS, READ_IO_ERROR, READ_LINE_NOT_FOUND } ReadStatus;

// i should have the balls to use my own malloc in my malloc
typedef struct {
  ReadStatus status;
  char line[1024];
  char type[64];
} StackLineResult;

[[noreturn]] static inline void panic(const char *msg) {
  static const char prefix[] = "PANIC: ";
  usize len = 0;
  while (msg[len] != '\0')
    len++;
  (void)s_write(2, prefix, sizeof(prefix) - 1);
  (void)s_write(2, msg, len);
  (void)s_write(2, "\n", 1);
  s_exit(127);
}
