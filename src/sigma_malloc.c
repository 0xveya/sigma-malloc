#include "../include/sigma_malloc.h"

#include "../include/buddy.h"
#include "../include/libc_wrappers.h"
#include "../include/slab.h"
#include "../include/utils/bzero.h"

static void *sigma_vtable_alloc(void *ctx, usize size, usize alignment) {
  return sigma_alloc(ctx, size, alignment);
}

static void *sigma_vtable_alloc_debug(void *ctx, usize size, usize alignment,
                                      const char *file, const char *func,
                                      i32 line) {
  return sigma_alloc_debug(ctx, size, alignment, file, func, line);
}

static void *sigma_vtable_calloc(void *ctx, usize count, usize size,
                                 usize alignment) {
  return sigma_calloc(ctx, count, size, alignment);
}

void *sigma_calloc(sigma_allocator_t *sigma, usize count, usize size,
                   usize alignment) {

  if (size != 0 && count > SIZE_MAX / size)
    return NULL;

  usize total = count * size;
  void *ptr = sigma_alloc(sigma, total, alignment);
  if (ptr != NULL && (!sigma->source->returns_zeroed_memory ||
                      alloc_header_from_user(ptr)->type != ALLOC_TYPE_LARGE))
    fill_zero(ptr, total);
  return ptr;
}

static void *sigma_vtable_calloc_debug(void *ctx, usize count, usize size,
                                       usize alignment, const char *file,
                                       const char *func, i32 line) {
  if (size != 0 && count > SIZE_MAX / size)
    return NULL;

  usize total = count * size;
  sigma_allocator_t *sigma = ctx;
  void *ptr = sigma_alloc_debug(sigma, total, alignment, file, func, line);
  if (ptr != NULL && (!sigma->source->returns_zeroed_memory ||
                      alloc_header_from_user(ptr)->type != ALLOC_TYPE_LARGE))
    fill_zero(ptr, total);
  return ptr;
}

static void *sigma_vtable_realloc(void *ctx, void *ptr, usize old_size,
                                  usize new_size, usize alignment) {
  return sigma_realloc(ctx, ptr, old_size, new_size, alignment);
}

static void *sigma_vtable_realloc_debug(void *ctx, void *ptr, usize old_size,
                                        usize new_size, usize alignment,
                                        const char *file, const char *func,
                                        i32 line) {
  if (ptr == NULL)
    return sigma_alloc_debug(ctx, new_size, alignment, file, func, line);
  return sigma_realloc(ctx, ptr, old_size, new_size, alignment);
}

static void sigma_vtable_free(void *ctx, void *ptr, usize size,
                              usize alignment) {
  (void)size;
  (void)alignment;
  sigma_free(ctx, ptr);
}

static const allocator_vtable_t g_sigma_allocator_vtable = {
    .alloc = sigma_vtable_alloc,
    .alloc_debug = sigma_vtable_alloc_debug,
    .calloc = sigma_vtable_calloc,
    .calloc_debug = sigma_vtable_calloc_debug,
    .realloc = sigma_vtable_realloc,
    .realloc_debug = sigma_vtable_realloc_debug,
    .free = sigma_vtable_free,
};

#if SIGMA_DEBUG
static void sigma_set_debug_info(void *ptr, const LeakInfo *info) {
  alloc_header_t *alloc = alloc_header_from_user(ptr);

  if (alloc->type == ALLOC_TYPE_SLAB) {
    obj_header_t *header = SIGMA_CONTAINER_OF(alloc, obj_header_t, header);
    header->alloc_file = info->file;
    header->alloc_func = info->func;
    header->alloc_line = info->line;
  } else if (alloc->type == ALLOC_TYPE_BUDDY) {
    buddy_header_t *header = SIGMA_CONTAINER_OF(alloc, buddy_header_t, header);
    header->alloc_file = info->file;
    header->alloc_func = info->func;
    header->alloc_line = info->line;
  } else if (alloc->type == ALLOC_TYPE_LARGE) {
    large_header_t *header = SIGMA_CONTAINER_OF(alloc, large_header_t, header);
    header->meta->alloc_file = info->file;
    header->meta->alloc_func = info->func;
    header->meta->alloc_line = info->line;
  }
}
#endif

allocator_t sigma_allocator(sigma_allocator_t *sigma) {
  return (allocator_t){
      .ctx = sigma,
      .vtable = &g_sigma_allocator_vtable,
  };
}

void *sigma_realloc(sigma_allocator_t *sigma, void *ptr, usize old_size,
                    usize new_size, usize alignment) {
  if (ptr == NULL)
    return sigma_alloc(sigma, new_size, alignment);
  if (new_size == 0) {
    sigma_free(sigma, ptr);
    return NULL;
  }

#if SIGMA_DEBUG
  LeakInfo debug_info = {0};
  bool has_debug_info = sigma_debug_allocation_info(ptr, &debug_info);
#endif

  void *new_ptr = sigma_alloc(sigma, new_size, alignment);
  if (new_ptr == NULL)
    return NULL;

#if SIGMA_DEBUG
  if (has_debug_info)
    sigma_set_debug_info(new_ptr, &debug_info);
#endif

  usize stored_size = alloc_header_from_user(ptr)->requested_size;
  usize copy_size = stored_size < new_size ? stored_size : new_size;
  if (old_size < copy_size)
    copy_size = old_size;
  sigma_memcpy(new_ptr, ptr, copy_size);
  sigma_free(sigma, ptr);
  return new_ptr;
}

void sigma_allocator_init(sigma_allocator_t *sigma,
                          const memory_source_t *source) {
  *sigma = (sigma_allocator_t){
      .initialized = true,
      .is_debug = SIGMA_DEBUG,
      .source = source,
      .large_allocs_head = NULL,
  };
}
