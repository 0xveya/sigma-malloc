#pragma once

#define ANSI_RED "\x1b[31m"
#define ANSI_DIM "\x1b[2m"
#define ANSI_BOLD "\x1b[1m"
#define ANSI_RESET "\x1b[0m"

typedef enum {
  READ_SUCCESS,
  READ_EOF,
  READ_IO_ERROR,
  READ_LINE_NOT_FOUND
} ReadStatus;

// i should have the balls to use my own malloc in my malloc
typedef struct {
  ReadStatus status;
  char line[1024];
  char type[64];
} StackLineResult;

typedef struct {
  ReadStatus status;
  char *type;

  union {
    char *line;
    int os_errno;
  } value;
} ReadLineResult;

static inline void sigma_defer_cleanup(void (^*block)(void)) {
  if (*block) {
    (*block)();
  }
}

// defer macro using clang blocks extension
#define defer_concat(a, b) a##b
#define defer_id(a) defer_concat(__defer_blk_, a)

#define defer                                                                  \
  void (^defer_id(__LINE__))(void)                                             \
      __attribute__((cleanup(sigma_defer_cleanup))) = ^
