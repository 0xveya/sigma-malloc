#include "parser.h"

static bool string_equal(const char *left, const char *right) {
  while (*left != '\0' && *left == *right) {
    left++;
    right++;
  }
  return *left == *right;
}

static void skip_spaces(parser_t *parser) {
  while (parser->input[parser->position] == ' ')
    parser->position++;
}

static char *copy_slice(parser_t *parser, usize start, usize end) {
  while (end > start && parser->input[end - 1] == ' ')
    end--;

  usize length = end - start;
  char *copy = allocator_array(parser->allocator, char, length + 1);
  if (copy == NULL)
    return NULL;

  for (usize i = 0; i < length; i++)
    copy[i] = parser->input[start + i];
  copy[length] = '\0';
  return copy;
}

static bool parser_push(parser_t *parser, parser_entry_t entry) {
  if (parser->entry_count == parser->entry_capacity) {
    usize old_capacity = parser->entry_capacity;
    usize new_capacity = old_capacity == 0 ? 8 : old_capacity * 2;
    /* The typed helper preserves entry alignment and the existing prefix. */
    parser_entry_t *entries = allocator_rearray(
        parser->allocator, parser->entries, old_capacity, new_capacity);
    if (entries == NULL)
      return false;
    parser->entries = entries;
    parser->entry_capacity = new_capacity;
  }

  parser->entries[parser->entry_count++] = entry;
  return true;
}

void parser_init(parser_t *parser, allocator_t allocator, const char *input) {
  *parser = (parser_t){
      .allocator = allocator,
      .input = input,
  };
}

bool parser_parse(parser_t *parser) {
  while (parser->input[parser->position] != '\0') {
    skip_spaces(parser);
    usize key_start = parser->position;

    while (parser->input[parser->position] != '=' &&
           parser->input[parser->position] != '\0')
      parser->position++;
    usize key_end = parser->position;
    if (key_start == key_end || parser->input[parser->position] != '=')
      return false;
    parser->position++;

    skip_spaces(parser);
    usize value_start = parser->position;
    while (parser->input[parser->position] != ';' &&
           parser->input[parser->position] != '\0')
      parser->position++;
    usize value_end = parser->position;
    if (value_start == value_end)
      return false;

    parser_entry_t entry = {
        .key = copy_slice(parser, key_start, key_end),
        .value = copy_slice(parser, value_start, value_end),
    };
    if (entry.key == NULL || entry.value == NULL || !parser_push(parser, entry))
      return false;

    if (parser->input[parser->position] == ';')
      parser->position++;
  }
  return true;
}

const char *parser_get(const parser_t *parser, const char *key) {
  for (usize i = 0; i < parser->entry_count; i++) {
    if (string_equal(parser->entries[i].key, key))
      return parser->entries[i].value;
  }
  return NULL;
}
