#include "sigma_malloc.h"

allocator_t g_alloc = {0};

static void allocator_init(void) {
  if (g_alloc.initialized)
    return;

  g_alloc.is_debug = false;

#if USE_DEBUG_ALLOC == 2
  g_alloc.is_debug = false;
#elif USE_DEBUG_ALLOC == 1
  g_alloc.is_debug = true;
#else
#ifndef __OPTIMIZE__
  g_alloc.is_debug = true;
#else
  g_alloc.is_debug = false;
#endif
#endif

  for (size_t i = 0; i < NUM_CACHES; i++) {
    g_alloc.caches[i].obj_size = g_size_classes[i];

    g_alloc.caches[i].partial = NULL;
    g_alloc.caches[i].full = NULL;
    g_alloc.caches[i].empty = NULL;
  }

  g_alloc.initialized = true;
}

static cache_t *get_cache(size_t size) {
  for (size_t i = 0; i < NUM_CACHES; i++) {
    if (size <= g_alloc.caches[i].obj_size)
      return &g_alloc.caches[i];
  }

  return NULL;
}

static bool is_ptr_in_freelist(free_node_t *head, void *ptr) {
  while (head) {
    if ((void *)head == ptr)
      return true;
    head = head->next;
  }
  return false;
}

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
      size_t len = type_end - type_start;
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

static slab_t *slab_create(cache_t *cache) {
  size_t data_size = ALIGN_UP(cache->obj_size, sizeof(void *));

  size_t slot_size = sizeof(obj_header_t) + data_size;

  void *mem = mmap(NULL, SLAB_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED)
    return NULL;

  slab_t *slab = (slab_t *)mem;
  slab->owner = cache;
  slab->prev = NULL;
  slab->next = NULL;
  slab->used = 0;
  slab->free_list = NULL;

  uint8_t *obj_start =
      (uint8_t *)mem + ALIGN_UP(sizeof(slab_t), sizeof(void *));
  size_t usable = SLAB_SIZE - (size_t)(obj_start - (uint8_t *)mem);

  slab->capacity = usable / slot_size;

  for (size_t i = 0; i < slab->capacity; i++) {
    uint8_t *slot_ptr = obj_start + (i * slot_size);

    obj_header_t *hdr = (obj_header_t *)slot_ptr;
    hdr->alloc_file = NULL;
    hdr->alloc_func = NULL;
    hdr->alloc_line = 0;
    hdr->slab = slab;

    free_node_t *node = (free_node_t *)(slot_ptr + sizeof(obj_header_t));

    node->next = slab->free_list;
    slab->free_list = node;
  }

  return slab;
}

static void slab_push(slab_t **head, slab_t *slab) {
  slab->prev = NULL;
  slab->next = *head;

  if (*head)
    (*head)->prev = slab;

  *head = slab;
}

static void slab_remove(slab_t **head, slab_t *slab) {
  if (slab->prev)
    slab->prev->next = slab->next;

  if (slab->next)
    slab->next->prev = slab->prev;

  if (*head == slab)
    *head = slab->next;

  slab->prev = NULL;
  slab->next = NULL;
}

static void *slab_alloc(size_t size) {
  cache_t *cache = get_cache(size);
  if (!cache)
    return NULL;
  slab_t *slab = cache->partial;
  if (!slab) {
    slab = slab_create(cache);

    if (!slab)
      return NULL;

    slab_push(&cache->partial, slab);
  }

  free_node_t *node = slab->free_list;

  slab->free_list = node->next;

  slab->used++;

  if (slab->used == slab->capacity) {
    slab_remove(&cache->partial, slab);
    slab_push(&cache->full, slab);
  }

  return node;
}

void cock(void *pp) {
  if (!pp)
    return;

  obj_header_t *hdr = (obj_header_t *)((uint8_t *)pp - sizeof(obj_header_t));
  hdr->alloc_file = NULL;
  hdr->alloc_func = NULL;
  hdr->alloc_line = 0;
  slab_t *slab = hdr->slab;
  cache_t *cache = slab->owner;

  bool was_full = (slab->used == slab->capacity);
  free_node_t *node = (free_node_t *)pp;

  node->next = slab->free_list;
  slab->free_list = node;

  slab->used--;

  if (was_full) {
    slab_remove(&cache->full, slab);
    slab_push(&cache->partial, slab);
  }

  if (slab->used == 0) {
    slab_remove(&cache->partial, slab);
    slab_push(&cache->empty, slab);
  }
}

void *balls_backend(size_t size) {
  if (!g_alloc.initialized)
    allocator_init();
  if (size <= MAX_SLAB_SIZE)
    return slab_alloc(size);

  return NULL;
}
