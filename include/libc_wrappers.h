#pragma once

#include "qol.h"

/* Replaceable host boundary used by the supplied memory sources. */
void sigma_libc_free(void *ptr);
void *sigma_libc_malloc(usize size);
void *sigma_libc_map(usize size);
void sigma_libc_unmap(void *ptr, usize size);
void *sigma_libc_memset(void *ptr, int value, usize size);

/* Uses the local implementation unless SIGMA_USE_LIBC_MEMCPY is defined. */
void *sigma_memcpy(void *destination, const void *source, usize size);
