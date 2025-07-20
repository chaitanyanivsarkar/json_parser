#ifndef JSON_PARSER_INCLUDED
#define JSON_PARSER_INCLUDED

#ifndef JP_SYMEXPORT
#define JP_SYMEXPORT
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

struct json_allocator {
  void *(*al_malloc)(ptrdiff_t sz, void *ctx);
  void (*al_free)(void *ptr, void *ctx);
  void *ctx;
};

struct json_source {
  void (*next)(void *);
  unsigned char (*get_byte)(void *);
  bool (*has_next)(void *);
  void *ctx;
};

struct json_string_source_ctx {
  unsigned char *json_string;
  ptrdiff_t len;
  ptrdiff_t cursor;
};

static void
str_next_byte(void *ctx)
{
  struct json_string_source_ctx *ss = ctx;
  ++ss->cursor;
}

static unsigned char
str_get_byte(void *ctx)
{
  struct json_string_source_ctx *ss = ctx;
  return ss->json_string[ss->cursor];
}

static bool
str_has_next_byte(void *ctx)
{
  struct json_string_source_ctx *ss = ctx;
  return (ss->cursor < ss->len);
}

struct json_string_source_ctx make_ss(unsigned char *str, ptrdiff_t len) {
  return (struct json_string_source_ctx){.json_string = str, .len = len, .cursor = 0};
}

struct json_source string_source_make(struct json_string_source_ctx *ctx) {
  return (struct json_source){ .next = str_next_byte, .get_byte = str_get_byte, .has_next = str_has_next_byte, .ctx = ctx};
}

#ifdef JP_USE_LIB_ALLOC
#include <stdlib.h>

void *
lib_malloc(ptrdiff_t sz, void *ctx)
{
  printf("allocating %ld bytes\n", sz);
  (void)ctx;
  return malloc(sz);
}
void
lib_free(void *ptr, void *ctx)
{
  (void)ctx;
  free(ptr);
}

struct json_allocator lib_allocator = { .al_malloc = lib_malloc, .al_free = lib_free, .ctx = NULL };
#endif

typedef enum {
    JSON_NULL, JSON_BOOL,
    JSON_NUMBER, JSON_STRING,
    JSON_ARRAY, JSON_OBJECT,
    JSON_ERROR
} Json_Type;

/*

struct json_parser;
typedef struct json_parser Json;

JP_SYMEXPORT Json * json_make_parser(struct json_source src, struct json_allocator al);
JP_SYMEXPORT void json_set_allocator(Json *json_parser, struct json_allocator al);
JP_SYMEXPORT void json_set_streaming(Json *json_parser, bool streaming);
JP_SYMEXPORT void json_set_max_depth(Json *json_parser, int max_depth);
JP_SYMEXPORT void json_destroy_parser(Json *ctx);
JP_SYMEXPORT void json_reset_parser(Json *ctx);

typedef struct {
  Json_Type type;
  void *opaque;
} Json_Value;

JP_SYMEXPORT Json_Value json_parse(Json *ctx);
JP_SYMEXPORT char * json_to_string(Json_Value jv);
JP_SYMEXPORT void json_destroy(Json_Value *jv, Json *ctx);

// Extraction procedures
JP_SYMEXPORT const char * json_error(Json_Value jv);
JP_SYMEXPORT double json_number(Json_Value jv);
JP_SYMEXPORT unsigned char * json_string(Json_Value jv);
JP_SYMEXPORT bool json_bool(Json_Value jv);
JP_SYMEXPORT const unsigned char ** json_object_keys(Json_Value jv, ptrdiff_t *out_len);
JP_SYMEXPORT Json_Value json_object_val(Json_Value jv, char *key);
JP_SYMEXPORT ptrdiff_t json_array_len(Json_Value jv);
JP_SYMEXPORT Json_Value json_array_at(Json_Value jv, ptrdiff_t i);
*/
#endif
