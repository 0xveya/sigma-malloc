#include "../include/debug.h"
#include "../include/qol.h"
#include "../include/sigma_malloc.h"
#include "../include/slab.h"
#include "../include/utils.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

StackLineResult get_leak_line(const char *filename, int linenum) {
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

  int current_line = 1;

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
  char *type_start = strstr(result.line, "sizeof(");
  if (type_start) {
    type_start += 7;
    char *type_end = strchr(type_start, ')');
    if (type_end) {
      size_t len = (size_t)(type_end - type_start);
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

#if SIGMA_DEBUG
[[gnu::destructor]]
void show_skill_issues(void) {
  if (!g_alloc.initialized || !g_alloc.is_debug) {
    return;
  }
  size_t leaks_count = 0;

  for (size_t i = 0; i < NUM_CACHES; i++) {
    cache_t *cache = &g_alloc.caches[i];
    slab_t *slabs_to_check[] = {cache->partial, cache->full};

    for (int s_idx = 0; s_idx < 2; s_idx++) {
      slab_t *current_slab = slabs_to_check[s_idx];
      while (current_slab) {

        uint8_t *obj_start =
            (uint8_t *)current_slab + ALIGN_UP(sizeof(slab_t), sizeof(void *));
        size_t data_size = ALIGN_UP(cache->obj_size, sizeof(void *));
        size_t slot_size = sizeof(obj_header_t) + data_size;

        for (size_t obj_i = 0; obj_i < current_slab->capacity; obj_i++) {
          uint8_t *slot_ptr = obj_start + (obj_i * slot_size);
          obj_header_t *hdr = (obj_header_t *)slot_ptr;
          void *user_ptr = slot_ptr + sizeof(obj_header_t);

          if (!is_ptr_in_freelist(current_slab->free_list, user_ptr)) {

            const char *display_file = hdr->alloc_file;
            const char *display_func = hdr->alloc_func;
            int display_line = hdr->alloc_line;

            if (hdr->alloc_file != NULL) {
              display_file = strrchr(hdr->alloc_file, '/')
                                 ? strrchr(hdr->alloc_file, '/') + 1
                                 : hdr->alloc_file;
              display_func = hdr->alloc_func;
              display_line = hdr->alloc_line;
            }
            var line_result = get_leak_line(display_file, display_line);
            if (line_result.status == READ_SUCCESS) {
              fprintf(stderr, ANSI_RED "Memory Leak Detected" ANSI_RESET ":\n");
              fprintf(stderr,
                      "  " ANSI_BOLD "Size Class:" ANSI_RESET
                      " %zu bytes of type" ANSI_BOLD " %s " ANSI_RESET "\n",
                      cache->obj_size, line_result.type);
              fprintf(stderr,
                      "  " ANSI_BOLD "Location:" ANSI_RESET "   " ANSI_DIM
                      "%s:" ANSI_RESET "%d" ANSI_DIM ":" ANSI_RESET
                      " inside " ANSI_BOLD "%s" ANSI_RESET "()\n",
                      display_file, display_line, display_func);
              fprintf(stderr, "     " ANSI_DIM "=>" ANSI_RESET " %s\n\n",
                      line_result.line);

              leaks_count++;
            } else {
              fprintf(stderr, ANSI_RED "Memory Leak Detected" ANSI_RESET ":\n");
              fprintf(stderr,
                      "  " ANSI_BOLD "Size Class:" ANSI_RESET
                      " %zu bytes of type" ANSI_BOLD " %s " ANSI_RESET "\n",
                      cache->obj_size, line_result.type);
              fprintf(stderr,
                      "  " ANSI_BOLD "Location:" ANSI_RESET "   " ANSI_DIM
                      "%s:" ANSI_RESET "%d" ANSI_DIM ":" ANSI_RESET
                      " inside " ANSI_BOLD "%s" ANSI_RESET "()\n",
                      display_file, display_line, display_func);
              fprintf(stderr,
                      "     " ANSI_DIM "=> (source line unavailable)\n\n");

              leaks_count++;
            }
          }
        }
        current_slab = current_slab->next;
      }
    }
  }

  if (leaks_count > 0) {
    if (HORNY_MODE == 0) {
      fprintf(stderr, "leaks: %zu \n", leaks_count);
    } else {
      fprintf(stderr, "you were a leaky bottom and leaked %zu times\n",
              leaks_count);
    }
  } else {
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
      break;
    }
  }
}
#endif
