#pragma once

#include "../qol.h"

/* Zero exactly n bytes using the selected local implementation. */
void fill_zero(void *ptr, usize n);
