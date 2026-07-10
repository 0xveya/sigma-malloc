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

  alloc_header_t *ah = (alloc_header_t *)((u8 *)pp - sizeof(alloc_header_t));

  switch (ah->type) {
  case ALLOC_TYPE_SLAB:
    slab_free(pp);
    break;

  case ALLOC_TYPE_BUDDY:
    printf("[COCK DEBUG] Routing to buddy_free: ptr %p | Magic: 0x%X\n", pp,
           ah->magic);
    fflush(stdout);
    buddy_free(&g_alloc.buddy_pool, pp);
    break;

  case ALLOC_TYPE_LARGE:
    large_free(pp);
    break;

  default:
    printf("[COCK FATAL] Invalid memory block signature: Type %d, Magic 0x%X\n",
           ah->type, ah->magic);
    fflush(stdout);
    panic("invalid pointer");
  }
}
