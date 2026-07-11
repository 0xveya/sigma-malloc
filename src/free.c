#include "../include/buddy.h"
#include "../include/large.h"
#include "../include/qol.h"
#include "../include/sigma_malloc.h"
#include "../include/slab.h"
#include "../include/utils.h"
#include <stdint.h>

void cock(void *pp) {
  if (!pp)
    return;

  alloc_header_t *ah = alloc_header_from_user(pp);

  switch (ah->type) {
  case ALLOC_TYPE_SLAB:
    slab_free(pp);
    break;

  case ALLOC_TYPE_BUDDY:
    buddy_free(&g_alloc.buddy_pool, pp);
    break;

  case ALLOC_TYPE_LARGE:
    large_free(pp);
    break;

  default:
    panic("invalid pointer");
  }
}
