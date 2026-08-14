#pragma once

#include "allocator_vtable.h"

typedef struct parser_entry {
  char *key;
  char *value;
} parser_entry_t;

/* Parser state owns entries and strings through the supplied allocator. */
typedef struct parser {
  allocator_t allocator;
  const char *input;
  usize position;
  parser_entry_t *entries;
  usize entry_count;
  usize entry_capacity;
} parser_t;

/* Attach allocator and input without allocating. Input must outlive parser. */
void parser_init(parser_t *parser, allocator_t allocator, const char *input);
/* Parse semicolon-separated key=value entries into allocator-owned strings. */
bool parser_parse(parser_t *parser);
/* Borrow a parsed value, or return NULL when key was not present. */
const char *parser_get(const parser_t *parser, const char *key);
