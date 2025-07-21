#ifndef JSON_PARSER_INCLUDED
#define JSON_PARSER_INCLUDED

#ifndef JP_SYMEXPORT
#define JP_SYMEXPORT
#endif

#include <stddef.h>
#include <stdbool.h>

struct json_allocator {
  void *(*al_malloc)(ptrdiff_t sz, void *ctx);
  void (*al_free)(void *ptr, void *ctx);
  void *ctx;
};

#ifdef JP_USE_LIB_ALLOC
#include <stdlib.h>

void *
lib_malloc(ptrdiff_t sz, void *ctx)
{
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

typedef struct {
  unsigned char *s;
  ptrdiff_t len;
} ustring;

struct json_source {
  void (*next)(void *);
  unsigned char (*get_byte)(void *);
  bool (*has_next)(void *);
  void *ctx;
};

typedef struct json_parser Json_Parser;
typedef struct json_ast_node Json_View;

Json_Parser * make_parser(struct json_source src, struct json_allocator al);
void json_parser_set_streaming(Json_Parser *p, bool streaming);
void json_parser_set_max_depth(Json_Parser *p, int max_depth);
const Json_View * json_parse(Json_Parser *p);
int json_parser_linenum(Json_Parser *p);
int json_parser_position(Json_Parser *p);
void json_parser_reset(Json_Parser *p);
void destroy_parser(Json_Parser *p);

Json_Type json_type(const Json_View *v);
double json_number(const Json_View *v);
bool json_bool(const Json_View *v);
ustring json_string(const Json_View *v);
ptrdiff_t json_array_len(const Json_View *v);
const Json_View * json_array_at(const Json_View *v, ptrdiff_t i);
const ustring* json_object_keys(const Json_View *v, ptrdiff_t *out_len);
const Json_View * json_object_val(const Json_View *v, const ustring key);
const char * json_error(const Json_View *v);
#endif
