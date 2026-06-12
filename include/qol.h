#pragma once

#include <stddef.h>
#include <stdint.h>

#define var auto
#define ALIGN_UP(x, a) (((x) + ((size_t)(a) - 1)) & ~((size_t)(a) - 1))

typedef size_t usize;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef uintptr_t uptr;
typedef intptr_t iptr;
