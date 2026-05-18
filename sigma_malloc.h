#ifndef SIGMA_MALLOC_H
# define SIGMA_MALLOC_H

#define var auto

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

#endif
