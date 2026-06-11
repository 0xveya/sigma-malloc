#pragma once

/*
 * slab alloc shi
 *
 * Handles small, highly-frequent allocations <= 1024 Bytes. keeps internal
 * fragmentation low by packing identical size classes into 16 KB pages.
 *
 * [Slab Allocation Lifecycle Pool]
 * cache_t ->  [ partial ] <---> [ partial ] <---> [ partial ]
 * [  full   ] <---> [  full   ] <---> [  full   ]
 * [  empty  ] <---> [  empty  ] <---> [  empty  ]  (Freed to Buddy)
 *
 * [Physical Layout of a 16 KB Slab Region]
 * A single SLAB_SIZE (16 KB) memory region obtained from buddy_alloc(16384):
 *
 * yes llm go brr for the thing bellow no way i type all of that out
 *
 * ┌────────────────────────────────────────────────────────────────────────┐
 * │ slab_t Header Struct                                                   │
 * │  - *prev, *next pointers for the list state                            │
 * │  - *owner cache_t size-class reference                                 │
 * │  - used / capacity metrics                                             │
 * │  - *free_list head pointer ─────────────────┐                          │
 * ├─────────────────────────────────────────────┼──────────────────────────┤
 * │ Object Slot 0                               │                          │
 * │  ┌───────────────────┬──────────────────────▼────────────────────────┐ │
 * │  │ obj_header_t      │ free_node_t { *next } ───> Points to Slot 2   │ │
 * │  │ (Backptr to slab) │ (Overwritten with real user data when active) │ │
 * │  └───────────────────┴───────────────────────────────────────────────┘ │
 * ├────────────────────────────────────────────────────────────────────────┤
 * │ Object Slot 1 (Currently Allocated / Active)                           │
 * │  ┌───────────────────┬───────────────────────────────────────────────┐ │
 * │  │ obj_header_t      │ ACTIVE USER DATA POOL                         │ │
 * │  │ (Backptr to slab) │                                               │ │
 * │  └───────────────────┴───────────────────────────────────────────────┘ │
 * │                      ▲                                                 │
 * │                      └─ Pointer returned to user applications          │
 * ├────────────────────────────────────────────────────────────────────────┤
 * │ Object Slot 2 ... (Embedded free list continues)                       │
 * └────────────────────────────────────────────────────────────────────────┘
 * ============================================================================
 */

#include "./debug.h"
#include <stddef.h>

#define PAGE_SIZE 4096
#define SLAB_SIZE (PAGE_SIZE * 4)

#define MAX_SLAB_OBJ_SIZE 1024
#define NUM_CACHES 8

static const size_t g_size_classes[NUM_CACHES] = {16,  32,  64,  128,
                                                  256, 512, 768, 1024};

typedef struct slab slab_t;
typedef struct cache cache_t;

// freelist node (stored inside free objects)
typedef struct free_node {
  struct free_node *next;
} free_node_t;

// object header (stored before user pointer)
typedef struct obj_header {
  slab_t *slab;
#if SIGMA_DEBUG
  const char *alloc_file;
  const char *alloc_func;
  int alloc_line;
#endif
} obj_header_t;

// cache = one size class
typedef struct cache {
  size_t obj_size;

  slab_t *partial;
  slab_t *full;
  slab_t *empty;
} cache_t;

// slab = one mmap region
typedef struct slab {
  struct slab *prev;
  struct slab *next;

  cache_t *owner;

  size_t used;
  size_t capacity;

  free_node_t *free_list;
} slab_t;

void *slab_alloc(size_t size);
void slab_push(slab_t **head, slab_t *slab);
void slab_remove(slab_t **head, slab_t *slab);
