#include <emmintrin.h>
#include <immintrin.h>

#include "../../include/libc_wrappers.h"
#include "../../include/utils/bzero.h"

typedef u16 u16_unaligned __attribute__((aligned(1), may_alias));

typedef u32 u32_unaligned __attribute__((aligned(1), may_alias));

typedef u64 u64_unaligned __attribute__((aligned(1), may_alias));

typedef void (*zero_fn)(void *, usize);

static void zero_sse2(void *ptr, usize n);
[[gnu::target("avx2")]]
static void zero_avx2(void *ptr, usize n);

/*
 * selected implementation
 *
 * null until zero_setup() is called.
 */
static zero_fn zero_impl = NULL;

/*
 * Tiny scalar zeroing.
 *
 * Used only below the SIMD width.
 *
 */
static inline void zero_small(u8 *p, usize n) {
  if (n == 0)
    return;

  if (n >= 8) {
    *(u64_unaligned *)p = 0;
    *(u64_unaligned *)(p + n - 8) = 0;
    return;
  }

  if (n >= 4) {
    *(u32_unaligned *)p = 0;
    *(u32_unaligned *)(p + n - 4) = 0;
    return;
  }

  if (n >= 2) {
    *(u16_unaligned *)p = 0;
    *(u16_unaligned *)(p + n - 2) = 0;
    return;
  }

  *p = 0;
}

/*
 * call this once during allocator/program/runtime initialization.
 *
 * x86-64 always gives SSE2 as baseline.
 * if AVX2 is available use the wider AVX2 implementation instead.
 */
static void zero_setup(void) {
#ifdef SIGMA_USE_CPU_DISPATCH
  if (__builtin_cpu_supports("avx2"))
    zero_impl = zero_avx2;
  else
    zero_impl = zero_sse2;
#else
  zero_impl = zero_sse2;
#endif
}

static inline void fill_zero_simd(void *p, usize n) { zero_impl(p, n); }

/*
 * SSE2 (Streaming SIMD Extensions 2)
 */
static void zero_sse2(void *ptr, usize n) {
  u8 *p = ptr;
  __m128i z = _mm_setzero_si128();

  // 0..16
  if (n < 16) {
    zero_small(p, n);
    return;
  }

  /*
   * 16..31
   *
   * First 16 + last 16.
   * they overlap when n < 32.
   */
  if (n < 32) {
    _mm_storeu_si128((__m128i *)p, z);
    _mm_storeu_si128((__m128i *)(p + n - 16), z);
    return;
  }

  /*
   * 32..63
   *
   * first 32 and last 32.
   */
  if (n < 64) {
    _mm_storeu_si128((__m128i *)(p + 0), z);
    _mm_storeu_si128((__m128i *)(p + 16), z);

    _mm_storeu_si128((__m128i *)(p + n - 32), z);
    _mm_storeu_si128((__m128i *)(p + n - 16), z);

    return;
  }

  u8 *tail = p + n - 64;

  _mm_storeu_si128((__m128i *)(tail + 0), z);
  _mm_storeu_si128((__m128i *)(tail + 16), z);
  _mm_storeu_si128((__m128i *)(tail + 32), z);
  _mm_storeu_si128((__m128i *)(tail + 48), z);

  while (p < tail) {
    _mm_storeu_si128((__m128i *)(p + 0), z);
    _mm_storeu_si128((__m128i *)(p + 16), z);
    _mm_storeu_si128((__m128i *)(p + 32), z);
    _mm_storeu_si128((__m128i *)(p + 48), z);

    p += 64;
  }
}

/*
 * AVX2 (Advanced Vector Extensions 2)
 */
[[gnu::target("avx2")]]
static void zero_avx2(void *ptr, usize n) {
  u8 *p = ptr;

  __m256i z = _mm256_setzero_si256();

  // 0..15
  if (n < 16) {
    zero_small(p, n);
    _mm256_zeroupper();
    return;
  }

  // 16..31
  if (n < 32) {
    __m128i z128 = _mm_setzero_si128();

    _mm_storeu_si128((__m128i *)p, z128);
    _mm_storeu_si128((__m128i *)(p + n - 16), z128);

    _mm256_zeroupper();
    return;
  }

  // 32..63
  if (n < 64) {
    _mm256_storeu_si256((__m256i *)p, z);
    _mm256_storeu_si256((__m256i *)(p + n - 32), z);

    _mm256_zeroupper();
    return;
  }

  // 64
  if (n == 64) {
    _mm256_storeu_si256((__m256i *)p, z);
    _mm256_storeu_si256((__m256i *)(p + 32), z);

    _mm256_zeroupper();
    return;
  }

  // 65..127
  if (n < 128) {
    _mm256_storeu_si256((__m256i *)(p + 0), z);
    _mm256_storeu_si256((__m256i *)(p + 32), z);

    _mm256_storeu_si256((__m256i *)(p + n - 64), z);
    _mm256_storeu_si256((__m256i *)(p + n - 32), z);

    _mm256_zeroupper();
    return;
  }

  u8 *tail = p + n - 128;

  _mm256_storeu_si256((__m256i *)(tail + 0), z);
  _mm256_storeu_si256((__m256i *)(tail + 32), z);
  _mm256_storeu_si256((__m256i *)(tail + 64), z);
  _mm256_storeu_si256((__m256i *)(tail + 96), z);

  while (p < tail) {
    _mm256_storeu_si256((__m256i *)(p + 0), z);
    _mm256_storeu_si256((__m256i *)(p + 32), z);
    _mm256_storeu_si256((__m256i *)(p + 64), z);
    _mm256_storeu_si256((__m256i *)(p + 96), z);

    p += 128;
  }

  _mm256_zeroupper();
}

[[gnu::always_inline]]
static inline void zero_words(void *ptr, usize n) {
  u8 *p = ptr;

  const usize W = sizeof(usize);
  const usize mask = W - 1;

  while (n && ((usize)p & mask)) {
    *p++ = 0;
    --n;
  }

  usize *w = (usize *)p;

  while (n >= 8 * W) {
    w[0] = 0;
    w[1] = 0;
    w[2] = 0;
    w[3] = 0;
    w[4] = 0;
    w[5] = 0;
    w[6] = 0;
    w[7] = 0;

    w += 8;
    n -= 8 * W;
  }

  while (n >= W) {
    *w++ = 0;
    n -= W;
  }

  p = (u8 *)w;

  while (n--)
    *p++ = 0;
}

void fill_zero(void *p, usize n) {
  if (zero_impl == NULL)
    zero_setup();

  if (n < 16 || n >= 64 * 1024) {
    zero_words(p, n);
    return;
  }

  fill_zero_simd(p, n);
}

#ifndef SIGMA_BZERO_BENCHMARK
#define SIGMA_BZERO_BENCHMARK 0
#endif

#if SIGMA_BZERO_BENCHMARK

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define BUFFER_SIZE 8192
#define GUARD_VALUE 0xA5

#ifndef SIGMA_BZERO_BENCH_BYTES
#define SIGMA_BZERO_BENCH_BYTES (512ULL * 1024 * 1024)
#endif

static int test_zero(usize offset, usize n) {
  u8 *buf = sigma_libc_malloc(BUFFER_SIZE);

  if (!buf) {
    fprintf(stderr, "malloc failed\n");
    exit(1);
  }

  for (usize i = 0; i < BUFFER_SIZE; i++)
    buf[i] = GUARD_VALUE;

  fill_zero(buf + offset, n);

  for (usize i = 0; i < offset; i++) {
    if (buf[i] != GUARD_VALUE) {
      printf("FAIL: n=%zu offset=%zu overwrote BEFORE region at byte %zu\n",
             (size_t)n, (size_t)offset, (size_t)i);
      sigma_libc_free(buf);
      return 0;
    }
  }

  for (usize i = 0; i < n; i++) {
    if (buf[offset + i] != 0) {
      printf("FAIL: n=%zu offset=%zu byte %zu was not zero\n", (size_t)n,
             (size_t)offset, (size_t)i);
      sigma_libc_free(buf);
      return 0;
    }
  }

  for (usize i = offset + n; i < BUFFER_SIZE; i++) {
    if (buf[i] != GUARD_VALUE) {
      printf("FAIL: n=%zu offset=%zu overwrote AFTER region at byte %zu\n",
             (size_t)n, (size_t)offset, (size_t)i);
      sigma_libc_free(buf);
      return 0;
    }
  }

  sigma_libc_free(buf);
  return 1;
}

static uint64_t now_ns(void) {
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);

  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline void observe_buffer(void *p) {
  __asm__ volatile("" : : "r"(p) : "memory");
}

static void bench(usize size) {
  u8 *buf = sigma_libc_malloc(size);

  if (!buf) {
    fprintf(stderr, "malloc failed\n");
    exit(1);
  }

  usize iterations = SIGMA_BZERO_BENCH_BYTES / size;

  if (iterations == 0)
    iterations = 1;

  sigma_libc_memset(buf, GUARD_VALUE, size);

  uint64_t start = now_ns();

  for (usize i = 0; i < iterations; i++) {
    fill_zero_simd(buf, size);
    observe_buffer(buf);
  }

  uint64_t simd_ns = now_ns() - start;

  sigma_libc_memset(buf, GUARD_VALUE, size);

  start = now_ns();

  for (usize i = 0; i < iterations; i++) {
    sigma_libc_memset(buf, 0, size);
    observe_buffer(buf);
  }

  uint64_t libc_ns = now_ns() - start;

  sigma_libc_memset(buf, GUARD_VALUE, size);

  start = now_ns();

  for (usize i = 0; i < iterations; i++) {
    zero_words(buf, size);
    observe_buffer(buf);
  }

  uint64_t shit_ns = now_ns() - start;

  sigma_libc_memset(buf, GUARD_VALUE, size);

  start = now_ns();

  for (usize i = 0; i < iterations; i++) {
    fill_zero(buf, size);
    observe_buffer(buf);
  }

  uint64_t fill_zero_ns = now_ns() - start;

  double total = (double)size * (double)iterations;

  double simd_gbs = total / (double)simd_ns;
  double libc_gbs = total / (double)libc_ns;
  double shit_gbs = total / (double)shit_ns;
  double fill_zero_gbs = total / (double)fill_zero_ns;

  printf("%8zu B | simd %8.2f GB/s | libc %8.2f GB/s"
         " | unga %8.2f GB/s | fill_zero %8.2f GB/s\n",
         (size_t)size, simd_gbs, libc_gbs, shit_gbs, fill_zero_gbs);

  sigma_libc_free(buf);
}

int main(void) {
  zero_setup();

  usize tests[] = {0,   1,   2,   3,   4,   7,    8,    15,   16,  17,
                   31,  32,  33,  63,  64,  65,   127,  128,  129, 255,
                   256, 257, 511, 512, 513, 1023, 1024, 1025, 4096};

  usize test_count = sizeof(tests) / sizeof(tests[0]);

  for (usize offset = 0; offset < 64; offset++) {
    for (usize t = 0; t < test_count; t++) {
      usize n = tests[t];

      if (offset + n >= BUFFER_SIZE)
        continue;

      if (!test_zero(offset, n))
        return 1;
    }
  }

  for (usize n = 0; n <= 4096; n++) {
    if (!test_zero(3, n))
      return 1;

    if (!test_zero(17, n))
      return 1;

    if (!test_zero(33, n))
      return 1;
  }

  printf("all zero tests passed :bawa:\n\n");

  usize sizes[] = {8,
                   16,
                   32,
                   64,
                   128,
                   256,
                   512,
                   1024,
                   2048,
                   4096,
                   8192,
                   16 * 1024,
                   32 * 1024,
                   64 * 1024,
                   128 * 1024,
                   256 * 1024,
                   512 * 1024,
                   1024 * 1024,
                   2 * 1024 * 1024,
                   4 * 1024 * 1024,
                   8 * 1024 * 1024};

  usize size_count = sizeof(sizes) / sizeof(sizes[0]);

  for (usize i = 0; i < size_count; i++)
    bench(sizes[i]);

  return 0;
}
#endif
