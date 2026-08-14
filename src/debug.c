#include "../include/debug.h"
#include "../include/arena.h"
#include "../include/qol.h"
#include "../include/sigma_malloc.h"
#include "../include/slab.h"
#include "../include/utils.h"
#include "../include/utils/bzero.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_LEAKS 4096

static LeakInfo g_leaks[MAX_LEAKS];
static usize g_leak_count;

#if SIGMA_DEBUG
static bool is_ptr_in_freelist(free_node_t *head, void *ptr) {
  while (head) {
    if ((void *)head == ptr)
      return true;
    head = head->next;
  }
  return false;
}
#endif

static StackLineResult get_leak_line(const char *filename, i32 linenum) {
  StackLineResult result;
  result.line[0] = '\0';

  if (filename == NULL || linenum < 1) {
    result.status = READ_IO_ERROR;
    return result;
  }

  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    result.status = READ_IO_ERROR;
    return result;
  }

  i32 current_line = 1;

  while (current_line <= linenum) {
    if (fgets(result.line, sizeof(result.line), file) == NULL) {
      if (feof(file)) {
        result.status = READ_LINE_NOT_FOUND;
      } else {
        result.status = READ_IO_ERROR;
      }
      fclose(file);
      return result;
    }

    if (current_line == linenum) {
      break;
    }

    current_line++;
  }
  result.type[0] = '\0';
  var *type_start = strstr(result.line, "sizeof(");
  if (type_start) {
    type_start += 7;
    var *type_end = strchr(type_start, ')');
    if (type_end) {
      usize len = (usize)(type_end - type_start);
      if (len >= sizeof(result.type))
        len = sizeof(result.type) - 1;
      strncpy(result.type, type_start, len);
      result.type[len] = '\0';
    }
  } else {
    result.type[0] = 'u';
    result.type[1] = 'n';
    result.type[2] = 'k';
    result.type[3] = 'n';
    result.type[4] = 'o';
    result.type[5] = 'w';
    result.type[6] = 'n';
    result.type[7] = '\0';
  }

  fclose(file);
  result.status = READ_SUCCESS;
  return result;
}

static void format_readable_size(char *buf, usize buf_size, usize bytes) {
  double size = (double)bytes;
  const char *units[] = {"B", "KB", "MB", "GB", "TB"};
  int unit_idx = 0;

  while (size >= 1024.0 && unit_idx < 4) {
    size /= 1024.0;
    unit_idx++;
  }

  if (unit_idx == 0) {
    snprintf(buf, buf_size, "%zu B", bytes);
  } else {
    snprintf(buf, buf_size, "%.2f %s", size, units[unit_idx]);
  }
}

static void leak_push(LeakInfo info) {
  if (g_leak_count < MAX_LEAKS)
    g_leaks[g_leak_count++] = info;
}

int sigma_debug_enabled(void) { return SIGMA_DEBUG; }

void sigma_debug_reset_leaks(void) {
  g_leak_count = 0;
  fill_zero(g_leaks, sizeof(g_leaks));
}

usize sigma_debug_leak_count(void) { return g_leak_count; }

bool sigma_debug_leak_info(usize index, LeakInfo *out) {
  if (out == NULL || index >= g_leak_count)
    return false;
  *out = g_leaks[index];
  return true;
}

bool sigma_debug_allocation_info(void *ptr, LeakInfo *out) {
  if (ptr == NULL || out == NULL)
    return false;

#if SIGMA_DEBUG
  alloc_header_t *alloc = alloc_header_from_user(ptr);
  const char *file = NULL;
  const char *func = NULL;
  i32 line = 0;

  if (alloc->type == ALLOC_TYPE_SLAB) {
    obj_header_t *header = SIGMA_CONTAINER_OF(alloc, obj_header_t, header);
    file = header->alloc_file;
    func = header->alloc_func;
    line = header->alloc_line;
  } else if (alloc->type == ALLOC_TYPE_BUDDY) {
    buddy_header_t *header = SIGMA_CONTAINER_OF(alloc, buddy_header_t, header);
    file = header->alloc_file;
    func = header->alloc_func;
    line = header->alloc_line;
  } else if (alloc->type == ALLOC_TYPE_LARGE) {
    large_header_t *header = SIGMA_CONTAINER_OF(alloc, large_header_t, header);
    file = header->meta->alloc_file;
    func = header->meta->alloc_func;
    line = header->meta->alloc_line;
  } else {
    return false;
  }

  *out = (LeakInfo){
      .file = file,
      .func = func,
      .line = line,
      .size = alloc->requested_size,
  };
  return true;
#else
  (void)ptr;
  (void)out;
  return false;
#endif
}

#if SIGMA_DEBUG
static void collect_slab_leaks(sigma_allocator_t *sigma, arena_t *arena) {
  if (arena == NULL) {
    return;
  }
  for (usize i = 0; i < NUM_CACHES; i++) {
    cache_t *cache = &arena->caches[i];
    slab_t *slabs_to_check[] = {cache->partial, cache->full};

    for (i32 s_idx = 0; s_idx < 2; s_idx++) {
      slab_t *slab = slabs_to_check[s_idx];
      while (slab) {
        u8 *obj_start =
            (u8 *)slab + ALIGN_UP(sizeof(slab_t), SLAB_MAX_ALIGNMENT);
        usize user_offset = ALIGN_UP(sizeof(obj_header_t), SLAB_MAX_ALIGNMENT);
        usize slot_size =
            user_offset + ALIGN_UP(cache->obj_size, SLAB_MAX_ALIGNMENT);

        for (usize j = 0; j < slab->capacity; j++) {
          u8 *slot = obj_start + j * slot_size;
          obj_header_t *hdr = (obj_header_t *)slot;
          void *user = slot + user_offset;

          if (!is_ptr_in_freelist(slab->free_list, user)) {
            bool is_large_tracking_infrastructure = false;
            large_node_t *curr_large = sigma->large_allocs_head;
            while (curr_large) {
              large_header_t *lh =
                  SIGMA_CONTAINER_OF(alloc_header_from_user(curr_large->user),
                                     large_header_t, header);
              if ((void *)curr_large == user || (void *)lh->meta == user) {
                is_large_tracking_infrastructure = true;
                break;
              }
              curr_large = curr_large->next;
            }

            if (is_large_tracking_infrastructure) {
              continue;
            }
            leak_push((LeakInfo){
                .file = hdr->alloc_file,
                .func = hdr->alloc_func,
                .line = hdr->alloc_line,
                .size = cache->obj_size,
            });
          }
        }
        slab = slab->next;
      }
    }
  }
}

static inline void *node_index_to_ptr(buddy_pool_t *pool, usize node,
                                      usize order) {
  usize depth = (BUDDY_NUM_ORDERS - 1) - order;
  usize first = (1ULL << depth) - 1;
  usize block_index = node - first;
  usize block_size = 1ULL << (order + BUDDY_MIN_ORDER);

  return (void *)((uptr)pool->usable_start + block_index * block_size);
}

static inline usize node_left(usize node) { return node * 2 + 1; }

static inline usize node_right(usize node) { return node * 2 + 2; }

static void traverse_buddy_nodes(buddy_pool_t *pool, usize index,
                                 usize relative_order) {
  if (relative_order >= BUDDY_NUM_ORDERS) {
    return;
  }

  buddy_node_state_t state = pool->tree[index];

  if (state == BUDDY_NODE_FULL) {
    void *block_ptr = node_index_to_ptr(pool, index, relative_order);
    buddy_header_t *hdr = *(buddy_header_t **)block_ptr;

    if (hdr->header.magic == BUDDY_MAGIC && hdr->order == relative_order &&
        !hdr->is_slab_region) {
      leak_push((LeakInfo){
          .file = hdr->alloc_file,
          .func = hdr->alloc_func,
          .line = hdr->alloc_line,
          .size = hdr->header.requested_size,
      });
    }
    return;
  }

  if (state == BUDDY_NODE_SPLIT && relative_order > 0) {
    usize left_child = node_left(index);
    usize right_child = node_right(index);

    traverse_buddy_nodes(pool, left_child, relative_order - 1);
    traverse_buddy_nodes(pool, right_child, relative_order - 1);
  }
}

static void collect_buddy_leaks(buddy_pool_t *pool) {
  if (!pool || !pool->tree)
    return;

  traverse_buddy_nodes(pool, 0, BUDDY_NUM_ORDERS - 1);
}

static void collect_arena_buddy_leaks(arena_t *arena) {
  if (arena == NULL) {
    return;
  }
  for (arena_extent_t *extent = arena->extents; extent != NULL;
       extent = extent->next) {
    collect_buddy_leaks(&extent->buddy);
  }
}

static void collect_large_leaks(sigma_allocator_t *sigma) {
  large_debug_list_lock();
  large_node_t *node = sigma->large_allocs_head;
  while (node) {
    large_header_t *header = SIGMA_CONTAINER_OF(
        alloc_header_from_user(node->user), large_header_t, header);
    large_metadata_t *meta = header->meta;

    leak_push((LeakInfo){
        .file = meta->alloc_file,
        .func = meta->alloc_func,
        .line = meta->alloc_line,
        .size = header->header.requested_size,
    });

    node = node->next;
  }
  large_debug_list_unlock();
}

#endif

usize sigma_debug_collect_leaks(sigma_allocator_t *sigma) {
  sigma_debug_reset_leaks();

#if SIGMA_DEBUG
  if (sigma == NULL || !sigma->initialized || !sigma->is_debug)
    return 0;

  collect_slab_leaks(sigma, arena_get_existing(sigma));
  collect_arena_buddy_leaks(arena_get_existing(sigma));
  collect_large_leaks(sigma);
#else
  (void)sigma;
#endif

  return g_leak_count;
}

void sigma_debug_report_leaks(sigma_allocator_t *sigma) {
#if SIGMA_DEBUG
  if (sigma == NULL || !sigma->initialized || !sigma->is_debug)
    return;

  sigma_debug_collect_leaks(sigma);

  for (usize i = 0; i < g_leak_count; i++) {
    LeakInfo *info = &g_leaks[i];
    StackLineResult src = get_leak_line(info->file, info->line);
    const char *display_file = info->file;
    if (display_file != NULL && strrchr(display_file, '/') != NULL)
      display_file = strrchr(display_file, '/') + 1;
    if (display_file == NULL)
      display_file = "unknown";
    char size_str[32];
    format_readable_size(size_str, sizeof(size_str), info->size);

    fprintf(stderr, ANSI_RED "Memory Leak Detected" ANSI_RESET ":\n");
    fprintf(stderr,
            "  " ANSI_BOLD "Size Class:" ANSI_RESET
            " %s bytes of type" ANSI_BOLD " %s " ANSI_RESET "\n",
            size_str, src.status == READ_SUCCESS ? src.type : "unknown");
    fprintf(stderr,
            "  " ANSI_BOLD "Location:" ANSI_RESET "   " ANSI_DIM
            "%s:" ANSI_RESET "%d" ANSI_DIM ":" ANSI_RESET " inside " ANSI_BOLD
            "%s" ANSI_RESET "()\n",
            display_file, info->line,
            info->func == NULL ? "unknown" : info->func);
    if (src.status == READ_SUCCESS)
      fprintf(stderr, "     " ANSI_DIM "=>" ANSI_RESET " %s\n\n", src.line);
    else
      fprintf(stderr, "     " ANSI_DIM "=> (source line unavailable)\n\n");
  }

  if (g_leak_count > 0) {
    if (HORNY_MODE == 0)
      fprintf(stderr, "leaks: %zu\n", g_leak_count);
    else
      fprintf(stderr, "you were a leaky bottom and leaked %zu times\n",
              g_leak_count);
  } else {
#ifndef SIGMA_TESTING
    switch (NO_LEAK_REWARD) {
    case 0:
      fprintf(stderr, "no leaks\n");
      break;
    case 1:
      fprintf(stderr, "no leaks good girl\n");
      break;
    case 2:
      fprintf(stderr, "no leaks good enby\n");
      break;
    case 3:
      fprintf(stderr, "no leaks good boy\n");
      fprintf(stderr, "no leaks good boy\n");
      break;
    }
#endif
  }
#else
  (void)sigma;
#endif
}
