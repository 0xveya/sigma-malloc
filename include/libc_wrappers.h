#pragma once

#include "qol.h"

/* Optional libc-backed memory source and local byte primitives. */
void *sigma_libc_memset(void *ptr, int value, usize size);

/* Copies bytes without relying on a hosted C implementation. */
void *sigma_memcpy(void *destination, const void *source, usize size);
