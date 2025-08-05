#ifndef JSON_PARSER_INCLUDED
#define JSON_PARSER_INCLUDED

#ifndef JP_SYMEXPORT
#define JP_SYMEXPORT
#endif

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    JSON_NULL, JSON_BOOL,
    JSON_NUMBER, JSON_STRING,
    JSON_ARRAY, JSON_OBJECT,
    JSON_ERROR
} Json_Type;

enum json_err {
  JSON_ERR_OBJ_CURLY_START,
  JSON_ERR_KEY_NOT_STRING,
  JSON_ERR_MULTIPLE_COLON,
  JSON_ERR_COLON_NOT_FOUND,
  JSON_ERR_INVALID_END,
  JSON_ERR_VALUE_END,
  JSON_ERR_OBJ_TRAILING_COMMA,
  JSON_ERR_ARR_TRAILING_COMMA,
  JSON_ERR_INVALID_START,
  JSON_ERR_EXPECTED_NULL,
  JSON_ERR_EXPECTED_TRUE,
  JSON_ERR_EXPECTED_FALSE,
  JSON_ERR_INVALID_UTF8,
  JSON_ERR_INVALID_ESCAPE_CHAR,
  JSON_ERR_INVALID_UNICODE_ESCAPE,
  JSON_ERR_LONE_SURROGATE,
  JSON_ERR_UNPAIRED_SURROGATE,
  JSON_ERR_LEADING_ZEROES,
  JSON_ERR_PLUS_SIGN,
  JSON_ERR_NO_DIGIT_BEFORE_DECIMAL,
  JSON_ERR_NO_DIGIT_BEFORE_EXPONENT,
  JSON_ERR_NOT_A_NUMBER,
  JSON_ERR_DOUBLE_DECIMAL,
  JSON_ERR_DOUBLE_EXPONENT,
  JSON_ERR_CATCH_ALL,
  JSON_ERR_OOM,
  JSON_ERR_ENUM_SIZE
};

typedef struct {
  unsigned char *s;
  ptrdiff_t len;
} ustring;

struct json_allocator {
  void *(*al_malloc)(ptrdiff_t sz, void *ctx);
  void (*al_free)(void *ptr, void *ctx);
  void *ctx;
};

struct json_source {
  void (*next)(struct json_source *src);
  unsigned char (*get_byte)(struct json_source *src);
  bool (*has_next)(struct json_source *src);
  void *ctx;

  // cursor is set to zero unless streaming is set
  // this is to allow the ability to reset the parser
  // and continue parsing on a large stream
  // len is ignored if streaming is set
  ptrdiff_t cursor;
  ptrdiff_t len;
};

typedef struct json_ast_node Json_View;
typedef struct arena Arena;

typedef struct json_parser {
  int line_num;
  int char_num;
  int max_depth;
  int flags;
  struct {Arena* arenas; ptrdiff_t len; ptrdiff_t cap;} pool;

  struct json_source source;
  struct json_allocator allocator;
} Json_Parser;


JP_SYMEXPORT void json_open_user(Json_Parser p[static 1], struct json_source src);
JP_SYMEXPORT void json_open_cstr(Json_Parser p[static 1], char *str);
JP_SYMEXPORT void json_open_buffer(Json_Parser p[static 1], unsigned char *buf, ptrdiff_t len);
JP_SYMEXPORT void json_set_streaming(Json_Parser p[static 1], bool streaming);
JP_SYMEXPORT void json_set_allocator(Json_Parser p[static 1], struct json_allocator al);
JP_SYMEXPORT void json_set_max_depth(Json_Parser p[static 1], int max_depth);
JP_SYMEXPORT const Json_View * json_parse(Json_Parser p[static 1]);
JP_SYMEXPORT int json_linenum(Json_Parser p[static 1]);
JP_SYMEXPORT int json_position(Json_Parser p[static 1]);
JP_SYMEXPORT void json_parser_reset(Json_Parser p[static 1]);
JP_SYMEXPORT void json_close(Json_Parser p[static 1]);

JP_SYMEXPORT Json_Type json_type(const Json_View *v);
JP_SYMEXPORT double json_number(const Json_View *v);
JP_SYMEXPORT bool json_bool(const Json_View *v);
JP_SYMEXPORT ustring json_string(const Json_View *v);
JP_SYMEXPORT ptrdiff_t json_array_len(const Json_View *v);
JP_SYMEXPORT const Json_View * json_array_at(const Json_View *v, ptrdiff_t i);
JP_SYMEXPORT const ustring* json_object_keys(const Json_View *v, ptrdiff_t *out_len);
JP_SYMEXPORT const Json_View * json_object_val(const Json_View *v, const ustring key);
JP_SYMEXPORT const char * json_error(const Json_View *v);
JP_SYMEXPORT enum json_err json_err_code(const Json_View *v);
#endif
