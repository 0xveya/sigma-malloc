#pragma once

#define var auto
#define ALIGN_UP(x, a) (((x) + ((size_t)(a) - 1)) & ~((size_t)(a) - 1))
