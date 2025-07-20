#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <float.h>
#include <stdio.h>

#include "json_parser.h"

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

struct json_ast_node {
  Json_Type type;

  union {
    bool b;
    double num;
    unsigned char *s;
    struct json_arr {
      struct json_ast_node *arr;
      ptrdiff_t len;
      ptrdiff_t cap;
    } vec;
    struct json_fields {
      unsigned char **keys;
      struct json_ast_node *vals;
      ptrdiff_t len;
      ptrdiff_t cap;
    } obj;
    enum json_err err_code;
  } value;
};

struct json_parser {
  int line_num;
  int char_num;
  int max_depth;

  struct json_source  source;
  struct json_allocator allocator;
  struct json_ast_node json_node;
};

static void free_json_node(struct json_ast_node node, struct json_parser *p);

static void
parser_free(struct json_parser ctx[static 1], void *ptr)
{
  ctx->allocator.al_free(ptr, ctx->allocator.ctx);
}

static void *
parser_malloc(struct json_parser ctx[static 1], ptrdiff_t sz)
{
  return ctx->allocator.al_malloc(sz, ctx->allocator.ctx);
}

static struct json_parser *
make_parser_from_string(unsigned char **str, ptrdiff_t len, struct json_allocator al)
{
  struct json_parser *p = al.al_malloc(sizeof(struct json_parser), al.ctx);
  p->allocator = al;
  p->max_depth = 200;
  p->line_num = 0;
  p->char_num = 0;

  struct json_string_source_ctx *ssc = al.al_malloc(sizeof(struct json_parser), al.ctx);
  *ssc = make_ss(*str, len);
  struct json_source src = string_source_make(ssc);
  p->source = src;
  
  return p;
}

typedef struct {
  ptrdiff_t len;
  ptrdiff_t cap;
  unsigned char *str;
} String_Builder;

static bool
sb_append_char(String_Builder *sb, unsigned char c, struct json_parser *ctx)
{
  if (sb->len >= sb->cap -1) {
    ptrdiff_t new_cap = sb->cap == 0 ? 16 : 2*sb->cap;
    unsigned char *tmp = parser_malloc(ctx, new_cap);
    memset(tmp, 0, new_cap);
    if (tmp == NULL) {
      // Allocation failure start the cleanup
      parser_free(ctx, sb->str);
      sb->cap = 0;
      sb->len = 0;
      return false;
    }
    if (sb->str) memcpy(tmp, sb->str, sb->cap);
    if (sb->str) parser_free(ctx, sb->str);
    sb->str = tmp;
    sb->cap = new_cap;
  }
  sb->str[sb->len++] = c;
  sb->str[sb->len] = '\0';
  return true;
}

static unsigned char *
sb_tostr(String_Builder *sb)
{
  unsigned char *res = sb->str;
  sb->cap = 0;
  sb->len = 0;
  sb->str = NULL;
  return res;
}

typedef struct {
  unsigned char hex[2];
  unsigned char ls[2];
  int cursor;
  int lscursor;
} utf16_builder;

typedef struct {
  unsigned char b1;
  unsigned char b2;
  unsigned char b3;
  unsigned char b4;
} utf8;

static bool
sb_append_utf8(String_Builder *sb, utf8 u8, struct json_parser *ctx)
{
  if (u8.b1 < 128) {
    return sb_append_char(sb, u8.b1, ctx);
  } else if (u8.b1 >= 0b11000000 && u8.b1 <= 0b11011111) {
    return sb_append_char(sb, u8.b1, ctx)
      && sb_append_char(sb, u8.b2, ctx);
  } else if (u8.b1 >= 0b11100000 && u8.b1 <= 0b11101111) {
    return sb_append_char(sb, u8.b1, ctx)
      && sb_append_char(sb, u8.b2, ctx)
      && sb_append_char(sb, u8.b3, ctx);
  } else if (u8.b1 >= 0b11110000 && u8.b1 <= 0b11110111) {
    return sb_append_char(sb, u8.b1, ctx)
      && sb_append_char(sb, u8.b2, ctx)
      && sb_append_char(sb, u8.b3, ctx)
      && sb_append_char(sb, u8.b4, ctx);
  } else {
    return false;
  }
}

static int
convert_to_hex_digit(unsigned char h)
{
  switch (h) {
  case '0': return 0;
  case '1': return 1;
  case '2': return 2;
  case '3': return 3;
  case '4': return 4;
  case '5': return 5;
  case '6': return 6;
  case '7': return 7;
  case '8': return 8;
  case '9': return 9;
  case 'a': case 'A': return 10;
  case 'b': case 'B': return 11;
  case 'c': case 'C': return 12;
  case 'd': case 'D': return 13;
  case 'e': case 'E': return 14;
  case 'f': case 'F': return 15;
  default: return -1;
  }
}

static void
ub_append_hex(utf16_builder *ub, unsigned char h)
{
  if (ub->cursor == 0) {
    ub->hex[0] = (convert_to_hex_digit(h)<<4);
  } else if (ub->cursor == 1) {
    ub->hex[0] = ub->hex[0] + convert_to_hex_digit(h);
  } else if (ub->cursor == 2) {
    ub->hex[1] = (convert_to_hex_digit(h)<<4);
  } else if (ub->cursor == 3) {
    ub->hex[1] = ub->hex[1] + convert_to_hex_digit(h);
  }
  ++ub->cursor;
}

static void
ub_append_lshex(utf16_builder *ub, unsigned char h)
{
  if (ub->lscursor == 0) {
    ub->ls[0] = (convert_to_hex_digit(h)<<4);
  } else if (ub->lscursor == 1) {
    ub->ls[0] = ub->ls[0] + convert_to_hex_digit(h);
  } else if (ub->lscursor == 2) {
    ub->ls[1] = (convert_to_hex_digit(h)<<4);
  } else if (ub->lscursor == 3) {
    ub->ls[1] = ub->ls[1] + convert_to_hex_digit(h);
  }
  ++ub->lscursor;
}
static utf8
ub_toutf8(utf16_builder ub)
{
  (void)ub;
  return (utf8){ .b1='a' };
}

typedef struct {
  int sign;
  int expsign;
  int exponent;
  int decimal;
  int integer;
  int ndecdigits;
  bool has_num;
} Num_Builder;

static long int
pow10_helper(int num)
{
  if (num == 0) return 1;
  if (num < 0) return 0;
  if (num == 1) return 10;

  long int tmp = pow10_helper(num/2);
  if (num % 2 == 0) return tmp*tmp;
  else return tmp*tmp*10;
}

static double
pow10(int num)
{
  return (double)1.0 * pow10_helper(num);
}

static Num_Builder
nb_negative(Num_Builder nb)
{
  nb.has_num = true;
  nb.sign = -1;
  return nb;
}
static Num_Builder
nb_negative_exp(Num_Builder nb)
{
  nb.has_num = true;
  nb.expsign = -1;
  return nb;
}
static Num_Builder
nb_append_exp(Num_Builder nb, char e)
{
  assert(e <= '9' && e >= '0');
  nb.has_num = true;
  if (nb.exponent <= (INT_MAX - (e - '0'))/10) nb.exponent = nb.exponent * 10 + (e - '0');
  else nb.exponent = INT_MAX;

  return nb;
}
static Num_Builder
nb_append_decimal(Num_Builder nb, char d)
{
  assert(d <= '9' && d >= '0');
  nb.has_num = true;
  if (nb.decimal <= (INT_MAX - (d - '0'))/10) {
    nb.decimal = nb.decimal * 10 + (d - '0');
    nb.ndecdigits += 1;
  }
  else nb.decimal = INT_MAX;
  return nb;
}
static Num_Builder
nb_append_int(Num_Builder nb, char i)
{
  assert(i <= '9' && i >= '0');
  nb.has_num = true;
  if (nb.integer <= (INT_MAX - (i - '0'))/10) nb.integer = nb.integer * 10 + (i - '0');
  else nb.integer = INT_MAX;
  return nb;
}

static double
nb_todouble(Num_Builder nb)
{
  if (nb.integer == INT_MAX || nb.decimal == INT_MAX) return 0.0;
  if (nb.exponent > 308) return 0.0;
  
  if (nb.expsign == -1
      && DBL_MIN*pow10(nb.exponent) > nb.integer + (nb.decimal/pow10(nb.ndecdigits))) return 0.0;
  if (nb.expsign == 1
      && DBL_MAX/pow10(nb.exponent) < nb.integer + (nb.decimal/pow10(nb.ndecdigits))) return 0.0;
  
  double result = (1.0*(nb.integer * nb.sign)
                   + ((double)nb.decimal)/pow10(nb.ndecdigits))
    * pow10(nb.exponent * nb.expsign);
  
  return result;
}

static char err_lookup_table[JSON_ERR_ENUM_SIZE][128] = {
  [JSON_ERR_OBJ_CURLY_START] = "ERROR::JSON objects must start with curly bracket.",
  [JSON_ERR_KEY_NOT_STRING] = "ERROR::JSON object key not string.",
  [JSON_ERR_MULTIPLE_COLON] = "ERROR::Multiple colons found between key value pair.",
  [JSON_ERR_COLON_NOT_FOUND] = "ERROR::Colon not found between key and value of object.",
  [JSON_ERR_INVALID_END] = "ERROR::JSON ended unexpectedly.",
  [JSON_ERR_VALUE_END] = "ERROR::JSON object values can only be followed by a comma or curly bracket.",
  [JSON_ERR_OBJ_TRAILING_COMMA] = "ERROR::Trailing commas are not allowed in JSON objects.",
  [JSON_ERR_ARR_TRAILING_COMMA] = "ERROR::Trailing commas are not allowed in JSON arrays.",
  [JSON_ERR_INVALID_START] = "ERROR::Unexpected starting character found.",
  [JSON_ERR_EXPECTED_NULL] = "ERROR::Did you mean null?",
  [JSON_ERR_EXPECTED_TRUE] = "ERROR::Did you mean true?",
  [JSON_ERR_EXPECTED_FALSE] = "ERROR::Did you mean false?",
  [JSON_ERR_INVALID_UTF8] = "ERROR::Invalid UTF-8 found in the string.",
  [JSON_ERR_INVALID_ESCAPE_CHAR] = "ERROR::Invalid escape character found.",
  [JSON_ERR_INVALID_UNICODE_ESCAPE] = "ERROR::Only hexadecimal digits allowed after \\u.",
  [JSON_ERR_LONE_SURROGATE] = "ERROR::Lone Low Surrogate in UTF-16 are not allowed.",
  [JSON_ERR_UNPAIRED_SURROGATE] = "ERROR::Unpaired High Surrogate in UTF-16 are not allowed.",
  [JSON_ERR_LEADING_ZEROES] = "ERROR::Leading zeroes are not allowed in JSON numbers.",
  [JSON_ERR_PLUS_SIGN] = "ERROR::Number cannot start with + sign.",
  [JSON_ERR_NO_DIGIT_BEFORE_DECIMAL] = "ERROR::Atleast one digit is required before decimal point.",
  [JSON_ERR_NO_DIGIT_BEFORE_EXPONENT] = "ERROR::Atleast one digit is required before exponent.",
  [JSON_ERR_NOT_A_NUMBER] = "ERROR::Expected a json number got a malformed one.",
  [JSON_ERR_DOUBLE_DECIMAL] = "ERROR::More than one decimal points not allowed in a number.",
  [JSON_ERR_DOUBLE_EXPONENT] = "ERROR::More than one exponent not allowed in a number.",
  [JSON_ERR_CATCH_ALL] = "ERROR::I have no idea but something went really wrong.",
  [JSON_ERR_OOM] = "ERROR::Cannot allocate more memory stopping everything.",
};

static unsigned char
get_byte(struct json_parser *p);

static void
next_byte(struct json_parser *p)
{
  if (get_byte(p) == '\n') {
    ++p->line_num;
    p->char_num = 0;
  } else {
    ++p->char_num;
  }
  p->source.next(p->source.ctx);
}

static unsigned char
get_byte(struct json_parser *p)
{
  return p->source.get_byte(p->source.ctx);
}

static bool
has_next_byte(struct json_parser *p)
{
  return p->source.has_next(p->source.ctx);
}

static bool
json_vec_append(struct json_arr *arr, struct json_ast_node node, struct json_parser *ctx)
{
  if (arr->cap <= arr->len) {
    ptrdiff_t old_cap = arr->cap;
    ptrdiff_t new_cap = (arr->cap == 0 ? 16 : 2*(arr->cap));
    void *tmp = parser_malloc(ctx, new_cap*sizeof(struct json_ast_node));
    if (tmp == NULL) {
      // Free everything if memory allocation fails
      for (ptrdiff_t i = 0; i < arr->len; ++i) free_json_node(arr->arr[i], ctx);
      parser_free(ctx, arr->arr);
      arr->arr = NULL;
      arr->len = 0;
      arr->cap = 0;
      return false;
    }
    memset(tmp, 0, new_cap*sizeof(struct json_ast_node));
    if (arr->arr) memcpy(tmp, arr->arr, old_cap*sizeof(struct json_ast_node));
    if (arr->arr) parser_free(ctx, arr->arr);
    arr->arr = tmp;
    arr->cap = new_cap;
  }
  arr->arr[arr->len++] = node;
  return true;
}

static bool
json_obj_append(struct json_fields *obj, unsigned char *key, struct json_ast_node val, struct json_parser *ctx)
{
  if (obj->cap <= obj->len) {
    ptrdiff_t new_cap = obj->cap == 0 ? 4 : 2*(obj->cap);
    void *tmp_keys = parser_malloc(ctx, sizeof(key)*(new_cap));
    void *tmp_vals = parser_malloc(ctx, sizeof(val)*(new_cap));
    if (!(tmp_keys && tmp_vals)) {
      for (ptrdiff_t i = 0; i < obj->len; ++i) {
        free_json_node(obj->vals[i], ctx);
        parser_free(ctx, obj->keys[i]);
      }
      parser_free(ctx, obj->keys);
      parser_free(ctx, obj->vals);
      obj->keys = NULL;
      obj->vals = NULL;
      obj->len = 0;
      obj->cap = 0;
      return false;
    }
    memset(tmp_keys, 0, new_cap*sizeof(key));
    memset(tmp_vals, 0, new_cap*sizeof(val));
    if (obj->keys) memcpy(tmp_keys, obj->keys, obj->cap*sizeof(key));
    if (obj->vals) memcpy(tmp_vals, obj->vals, obj->cap*sizeof(val));
    if (obj->keys) parser_free(ctx, obj->keys);
    if (obj->vals) parser_free(ctx, obj->vals);
    obj->keys = tmp_keys;
    obj->vals = tmp_vals;
    obj->cap = new_cap;
  }
  obj->keys[obj->len] = key;
  obj->vals[obj->len] = val;
  obj->len++;
  return true;
}

static bool is_ws(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }

static void
skip_whitespace(struct json_parser ctx[static 1])
{
  while(has_next_byte(ctx) && is_ws(get_byte(ctx))) next_byte(ctx);
}
static struct json_ast_node parse_base_value(struct json_parser ctx[static 1]);
static struct json_ast_node parse_array(struct json_parser ctx[static 1]);
static struct json_ast_node parse_object(struct json_parser ctx[static 1]);
static struct json_ast_node make_json_error(enum json_err err_code);
static struct json_ast_node make_json_null();

static struct json_ast_node
parse_json_value(struct json_parser ctx[static 1])
{
  skip_whitespace(ctx);
  if (ctx->max_depth <= 0) return make_json_error(JSON_ERR_OOM);
  --ctx->max_depth;
  switch(get_byte(ctx)) {
  case '{': return parse_object(ctx);
  case '[': return parse_array(ctx);
  case '"':
  case '-':
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
  case 'n':
  case 't':
  case 'f':
    return parse_base_value(ctx);
  default: return make_json_error(JSON_ERR_INVALID_START);
  }
}

extern void
parse_json(struct json_parser p[static 1])
{
  p->json_node = parse_json_value(p);
  if (p->json_node.type != JSON_NUMBER) {
    next_byte(p);
  }
  skip_whitespace(p);
  if (has_next_byte(p)) {
    free_json_node(p->json_node, p);
    p->json_node = make_json_error(JSON_ERR_INVALID_END);
  }
}

static struct json_ast_node
make_json_empty_object(void)
{
  struct json_ast_node node = {.type = JSON_OBJECT};
  return node;
}

static struct json_ast_node
make_json_empty_array(void)
{
  struct json_ast_node node = { .type = JSON_ARRAY };
  return node;
}

static void
free_json_node(struct json_ast_node node, struct json_parser *p)
{
  switch (node.type) {
  case JSON_NUMBER: return;
  case JSON_BOOL: return;
  case JSON_NULL: return;
  case JSON_STRING:
    if (node.value.s) parser_free(p, (void *)node.value.s);
    return;
    
  case JSON_OBJECT: 
    for (ptrdiff_t i = 0; i < node.value.obj.len; ++i) {
      parser_free(p, node.value.obj.keys[i]);
      free_json_node(node.value.obj.vals[i], p);
    }
    if (node.value.obj.keys) parser_free(p, node.value.obj.keys);
    if (node.value.obj.vals) parser_free(p, node.value.obj.vals);
    return;
  
  case JSON_ARRAY:
    for (ptrdiff_t i = 0; i < node.value.vec.len; ++i) {
      free_json_node(node.value.vec.arr[i], p);
    }
    if (node.value.vec.arr) parser_free(p, node.value.vec.arr);
    return;
    
  case JSON_ERROR: return;
  }
}


static struct json_ast_node
parse_object(struct json_parser ctx[static 1])
{
  if (get_byte(ctx) == '{') {
    next_byte(ctx);
    skip_whitespace(ctx);
    if (get_byte(ctx) == '}') return make_json_empty_object();
    else {
      struct json_ast_node obj_node = make_json_empty_object();
      while (has_next_byte(ctx)) {
        switch (get_byte(ctx)) {
        case '"': {
          struct json_ast_node key_node = parse_base_value(ctx);
          if (key_node.type == JSON_ERROR) {
            free_json_node(obj_node, ctx);
            return key_node;
          }
          next_byte(ctx);
          skip_whitespace(ctx);

          if (get_byte(ctx) != ':') {
            free_json_node(key_node, ctx);
            free_json_node(obj_node, ctx);
            return make_json_error(JSON_ERR_COLON_NOT_FOUND);
          }

          next_byte(ctx);
          skip_whitespace(ctx);
        
          struct json_ast_node val_node = parse_json_value(ctx);
          if (val_node.type == JSON_ERROR) {
            free_json_node(key_node, ctx);
            free_json_node(obj_node, ctx);
            return val_node;
          }
          unsigned char *key = key_node.value.s;
          key_node.value.s = NULL;
          json_obj_append(&(obj_node.value.obj), key, val_node, ctx);
          
          if (val_node.type != JSON_NUMBER) {
            next_byte(ctx);
          }
          skip_whitespace(ctx);
          
          val_node = (struct json_ast_node){0};

          if (get_byte(ctx) == ',') {
            next_byte(ctx);
            skip_whitespace(ctx);
          } else if (get_byte(ctx) == '}') {
            return obj_node;
          } else {
            free_json_node(obj_node, ctx);
            return make_json_error(JSON_ERR_OBJ_TRAILING_COMMA);
          }
          
          break;
        }
        default:
          free_json_node(obj_node, ctx);
          return make_json_error(JSON_ERR_KEY_NOT_STRING);
        }
      }
      free_json_node(obj_node, ctx);
      return make_json_error(JSON_ERR_INVALID_END);
    }
  }
  else {
    return make_json_error(JSON_ERR_INVALID_START);
  }
}

static struct json_ast_node
parse_array(struct json_parser ctx[static 1])
{
  if (get_byte(ctx) == '[') {
    next_byte(ctx);
    skip_whitespace(ctx);
    if (get_byte(ctx) == ']') return make_json_empty_array();
    else {
      struct json_ast_node arr_node = make_json_empty_array();
      while (has_next_byte(ctx)) {
        switch (get_byte(ctx)) {
        case '"': case '-': case 't': case 'f': case 'n':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case '{': case '[':
          struct json_ast_node val = parse_json_value(ctx);
          if (val.type == JSON_ERROR) {
            free_json_node(arr_node, ctx);
            return val;
          }
          json_vec_append(&(arr_node.value.vec), val, ctx);

          if (val.type != JSON_NUMBER) {
            next_byte(ctx);
          }
          skip_whitespace(ctx);
          break;
        default:
          free_json_node(arr_node, ctx);
          return make_json_error(JSON_ERR_ARR_TRAILING_COMMA);
        }
        
        if (get_byte(ctx) == ',') {
          next_byte(ctx);
          skip_whitespace(ctx);  
        } else if (get_byte(ctx) == ']') {
          return arr_node;
        } else {
          free_json_node(arr_node, ctx);
          return make_json_error(JSON_ERR_INVALID_END);
        }
      }
      free_json_node(arr_node, ctx);
      return make_json_error(JSON_ERR_INVALID_END);
    }
  }
  else {
    return make_json_error(JSON_ERR_INVALID_START);
  }
}

enum json_bv_state {
    START,
    ERR_INVALID_START,
    NULL_N,
    NULL_U,
    NULL_L1,
    NULL_L2,
    ERR_EXPECTED_NULL,
    BOOL_T,
    BOOL_R,
    BOOL_U,
    BOOL_TE,
    BOOL_F,
    BOOL_A,
    BOOL_L,
    BOOL_S,
    BOOL_FE,
    ERR_EXPECTED_TRUE,
    ERR_EXPECTED_FALSE,
    STR_QUOTE_BEGIN,
    STR_UTF_1BYTE,
    STR_UTF_2BYTE_1,
    STR_UTF_2BYTE_2,
    STR_UTF_3BYTE_1,
    STR_UTF_3BYTE_2,
    STR_UTF_3BYTE_3,
    STR_UTF_4BYTE_1,
    STR_UTF_4BYTE_2,
    STR_UTF_4BYTE_3,
    STR_UTF_4BYTE_4,
    ERR_INVALID_UTF8,
    STR_CONTROL_SLASH,
    STR_CONTROL_QUOTE,
    STR_CONTROL_REVSOL,
    STR_CONTROL_SOL,
    STR_CONTROL_B,
    STR_CONTROL_F,
    STR_CONTROL_N,
    STR_CONTROL_R,
    STR_CONTROL_T,
    ERR_INVALID_ESCAPE_CHAR,
    STR_CONTROL_U,
    STR_UNICODE_HEX1,
    STR_UNICODE_HEX2,
    STR_UNICODE_HEX3,
    STR_UNICODE_HEX4,
    STR_UNICODE_MAYBE_HS_HEX1,
    STR_UNICODE_HS_HEX2,
    STR_UNICODE_HS_HEX3,
    STR_UNICODE_HS_HEX4,
    STR_UNICODE_LS_CONTROL_SLASH,
    STR_UNICODE_LS_CONTROL_U,
    STR_UNICODE_LS_HEX1,
    STR_UNICODE_LS_HEX2,
    STR_UNICODE_LS_HEX3,
    STR_UNICODE_LS_HEX4,
    ERR_INVALID_UNICODE_ESCAPE,
    ERR_LONE_SURROGATE,
    ERR_UNPAIRED_SURROGATE,
    STR_QUOTE_END,
    NUM_MINUS,
    NUM_ZERO,
    NUM_DIGIT19,
    NUM_DIGIT,
    NUM_DECIMAL,
    NUM_DECIMAL_DIGIT,
    NUM_EXPONENT,
    NUM_EXP_PLUS,
    NUM_EXP_MINUS,
    NUM_EXP_DIGIT,
    ERR_LEADING_ZEROES,
    ERR_PLUS_SIGN,
    ERR_NO_DIGIT_BEFORE_DECIMAL,
    ERR_NO_DIGIT_BEFORE_EXPONENT,
    ERR_NOT_A_NUMBER,
    ERR_DOUBLE_DECIMAL,
    ERR_DOUBLE_EXPONENT,
    NUM_END,
    JSON_BV_STATE_SIZE
};

static const enum json_bv_state transition_table[JSON_BV_STATE_SIZE+1][256] = {
  #include "bv_transition.table"
};

static struct json_ast_node
make_json_null()
{
  struct json_ast_node node = { .type = JSON_NULL };
  return node;
}

static struct json_ast_node
make_json_number(double num)
{
  struct json_ast_node node = { .type = JSON_NUMBER };
  node.value.num = num;
  return node;
}

static struct json_ast_node
make_json_bool(bool b)
{
  struct json_ast_node node = { .type = JSON_BOOL };
  node.value.b = b;
  return node;
}

static struct json_ast_node
make_json_error(enum json_err err_code)
{
  struct json_ast_node node = { .type = JSON_ERROR, .value.err_code = err_code };
  return node;
}

static struct json_ast_node
make_json_string(unsigned char *str)
{
  struct json_ast_node node = { .type = JSON_STRING };
  node.value.s = str;
  return node;
}

static struct json_ast_node
parse_base_value(struct json_parser ctx[static 1])
{
  enum json_bv_state current_state = START;
  String_Builder sb = {0};
  Num_Builder nb = {1, 1, 0, 0, 0, 0, false};
  utf16_builder ub = {0};
  unsigned char c;
  while (has_next_byte(ctx)) {
    c = get_byte(ctx);
    current_state = transition_table[current_state][c];
    switch (current_state) {
    case START: break;
    case ERR_INVALID_START:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_INVALID_START);
    case NULL_N: break;
    case NULL_U: break;
    case NULL_L1: break;
    case NULL_L2:
      return make_json_null();
      
    case ERR_EXPECTED_NULL:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_EXPECTED_NULL);
    case BOOL_T: break;
    case BOOL_R: break;
    case BOOL_U: break;
    case BOOL_TE:
      return make_json_bool(true);

    case BOOL_F: break;
    case BOOL_A: break;
    case BOOL_L: break;
    case BOOL_S: break;
    case BOOL_FE:
      return make_json_bool(false);

    case ERR_EXPECTED_TRUE:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_EXPECTED_TRUE);
    case ERR_EXPECTED_FALSE:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_EXPECTED_FALSE);

    case STR_QUOTE_BEGIN:
      break;

    case STR_UTF_1BYTE:
      if (!sb_append_char(&sb, c, ctx))
        return make_json_error(JSON_ERR_OOM);
      break; 
    case STR_UTF_2BYTE_1:
      if (!sb_append_char(&sb, c, ctx))
        return make_json_error(JSON_ERR_OOM);
      break;
    case STR_UTF_2BYTE_2:
      if (!sb_append_char(&sb, c, ctx))
        return make_json_error(JSON_ERR_OOM);
      break;
    case STR_UTF_3BYTE_1:
      if (!sb_append_char(&sb, c, ctx))
        return make_json_error(JSON_ERR_OOM);
      break;
    case STR_UTF_3BYTE_2:
      if (!sb_append_char(&sb, c, ctx))
        return make_json_error(JSON_ERR_OOM);
      break;
    case STR_UTF_3BYTE_3:
      if (!sb_append_char(&sb, c, ctx))
        return make_json_error(JSON_ERR_OOM);
      break;
    case STR_UTF_4BYTE_1:
      if (!sb_append_char(&sb, c, ctx))
        return make_json_error(JSON_ERR_OOM);
      break;
    case STR_UTF_4BYTE_2:
      if (!sb_append_char(&sb, c, ctx))
        return make_json_error(JSON_ERR_OOM);
      break;
    case STR_UTF_4BYTE_3:
      if (!sb_append_char(&sb, c, ctx))
        return make_json_error(JSON_ERR_OOM);
      break;
    case STR_UTF_4BYTE_4:
      if (!sb_append_char(&sb, c, ctx))
        return make_json_error(JSON_ERR_OOM);
      break;

    case ERR_INVALID_UTF8:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_INVALID_UTF8);

    case STR_CONTROL_SLASH: break;
    case STR_CONTROL_QUOTE:
      if (!sb_append_char(&sb, '"', ctx))
        return make_json_error(JSON_ERR_OOM);
      break;
    case STR_CONTROL_REVSOL:
      if (!sb_append_char(&sb, '\\', ctx))
        return make_json_error(JSON_ERR_OOM);
      break; 
    case STR_CONTROL_SOL:
      if (!sb_append_char(&sb, '/', ctx))
        return make_json_error(JSON_ERR_OOM);
      break; 
    case STR_CONTROL_B:
      if (!sb_append_char(&sb, '\b', ctx))
        return make_json_error(JSON_ERR_OOM);
      break; 
    case STR_CONTROL_F:
      if (!sb_append_char(&sb, '\f', ctx))
        return make_json_error(JSON_ERR_OOM);
      break; 
    case STR_CONTROL_N:
      if (!sb_append_char(&sb, '\n', ctx))
        return make_json_error(JSON_ERR_OOM);
      break; 
    case STR_CONTROL_R:
      if (!sb_append_char(&sb, '\r', ctx))
        return make_json_error(JSON_ERR_OOM);
      break; 
    case STR_CONTROL_T:
      if (!sb_append_char(&sb, '\t', ctx))
        return make_json_error(JSON_ERR_OOM);
      break; 

    case ERR_INVALID_ESCAPE_CHAR:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_INVALID_ESCAPE_CHAR);

    case STR_CONTROL_U: break;
    case STR_UNICODE_HEX1:
      ub_append_hex(&ub, c);
      break; 
    case STR_UNICODE_HEX2:
      ub_append_hex(&ub, c);
      break; 
    case STR_UNICODE_HEX3:
      ub_append_hex(&ub, c);
      break; 
    case STR_UNICODE_HEX4:
      ub_append_hex(&ub, c);
      sb_append_utf8(&sb, ub_toutf8(ub), ctx);
      break; 
    case STR_UNICODE_MAYBE_HS_HEX1:
      ub_append_hex(&ub, c);
      break; 
    case STR_UNICODE_HS_HEX2:
      ub_append_hex(&ub, c);
      break; 
    case STR_UNICODE_HS_HEX3:
      ub_append_hex(&ub, c);
      break; 
    case STR_UNICODE_HS_HEX4:
      ub_append_hex(&ub, c);
      break; 
    case STR_UNICODE_LS_CONTROL_SLASH: break;
    case STR_UNICODE_LS_CONTROL_U: break;
    case STR_UNICODE_LS_HEX1:
      ub_append_lshex(&ub, c);
      break; 
    case STR_UNICODE_LS_HEX2:
      ub_append_lshex(&ub, c);
      break; 
    case STR_UNICODE_LS_HEX3:
      ub_append_lshex(&ub, c);
      break; 
    case STR_UNICODE_LS_HEX4:
      ub_append_lshex(&ub, c);
      sb_append_utf8(&sb, ub_toutf8(ub), ctx);
      break; 

    case ERR_INVALID_UNICODE_ESCAPE:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_INVALID_UNICODE_ESCAPE);
    case ERR_LONE_SURROGATE:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_LONE_SURROGATE);
    case ERR_UNPAIRED_SURROGATE:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_UNPAIRED_SURROGATE);

    case STR_QUOTE_END:
      return make_json_string(sb_tostr(&sb));

    case NUM_MINUS:
      nb = nb_negative(nb);
      break; 
    case NUM_ZERO:
      nb = nb_append_int(nb, c);
      break; 
    case NUM_DIGIT19:
      nb = nb_append_int(nb, c);
      break; 
    case NUM_DIGIT:
      nb = nb_append_int(nb, c);
      break; 

    case NUM_DECIMAL: break;
    case NUM_DECIMAL_DIGIT:
      nb = nb_append_decimal(nb, c);
      break; 

    case NUM_EXPONENT: break;
    case NUM_EXP_PLUS: break;
    case NUM_EXP_MINUS:
      nb = nb_negative_exp(nb);
      break; 
    case NUM_EXP_DIGIT:
      nb = nb_append_exp(nb, c);
      break; 

    case ERR_LEADING_ZEROES:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_LEADING_ZEROES);
    case ERR_PLUS_SIGN:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_PLUS_SIGN);
    case ERR_NO_DIGIT_BEFORE_DECIMAL:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_NO_DIGIT_BEFORE_DECIMAL);
    case ERR_NO_DIGIT_BEFORE_EXPONENT:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_NO_DIGIT_BEFORE_EXPONENT);
    case ERR_NOT_A_NUMBER:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_NOT_A_NUMBER);
    case ERR_DOUBLE_DECIMAL:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_DOUBLE_DECIMAL);
    case ERR_DOUBLE_EXPONENT:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_DOUBLE_EXPONENT);
    case NUM_END:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_number(nb_todouble(nb));
    case JSON_BV_STATE_SIZE:
      if (sb.str) parser_free(ctx, sb.str);
      return make_json_error(JSON_ERR_CATCH_ALL);
    }
    next_byte(ctx);
  }
  //  printf("\n");
  // To handle the case where the json string is just a number since numbers don't have a fixed ending character
  if (sb.str) parser_free(ctx, sb.str);
  if (nb.has_num) return make_json_number(nb_todouble(nb));
  else return make_json_error(JSON_ERR_INVALID_END);
}


// TODO: Complete the implementation for UTF-16 to UTF-8 conversion
// DONE: Run and pass the complete json test suite and measure performance as well
// DONE: Add a top level parse_json() function that takes a json_source object and returns a parser handle
// DONE: Add line and cursor number information to the parser to improve error handling
// DONE: Create a json_source ADT that can take either a string or a file or a stream
// TODO: Create a final header file along with a statically linked library compiled with -Ofast and -pgo
// DONE: Add custom allocator support
// TODO: Change the object_fields from being a pair of arrays to a msi hash_map (both double size at full so memory usage will be the same)
// DONE: Centralize the error handling.

#ifdef TEST

#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

static void
print_json_node_helper(const struct json_parser *p, const struct json_ast_node node, int level)
{
  switch (node.type) {
  case JSON_NULL:
    printf("null");
    break;
  case JSON_BOOL:
    printf("%s", (node.value.b ? "true" : "false"));
    break;
  case JSON_ARRAY:
    printf("[\n");
    for (int i = 0; i < level; ++i) printf("\t");
    if (node.value.vec.len) {
      print_json_node_helper(p, node.value.vec.arr[0], level+1);
    }

    for (ptrdiff_t i = 1; i < node.value.vec.len; ++i) {
      printf(",\n");
      for (int i = 0; i < level; ++i) printf("\t");
      print_json_node_helper(p, node.value.vec.arr[i], level+1);
    }
    printf("\n");
    for (int i = 0; i < level-1; ++i) printf("\t");
    printf("]");
    break;
  case JSON_OBJECT:
    printf("{\n");
    for (int i = 0; i < level; ++i) printf("\t");
    if (node.value.obj.len) {
      printf("\"%s\"", (char *)node.value.obj.keys[0]);
      printf(" : ");
      print_json_node_helper(p, node.value.obj.vals[0], level+1);
    }

    for(ptrdiff_t i = 1; i < node.value.obj.len; ++i) {
      printf(",\n");
      for (int i = 0; i < level; ++i) printf("\t");
      printf("\"%s\"", (char *)node.value.obj.keys[i]);
      printf(" : ");
      print_json_node_helper(p, node.value.obj.vals[i], level+1);
    }
    printf("\n");
    for (int i = 0; i < level-1; ++i) printf("\t");
    printf("}");
    break;
  case JSON_NUMBER:
    printf("%lf", node.value.num);
    break;
  case JSON_STRING:
    printf("\"%s\"", (char *)node.value.s);
    break;
  case JSON_ERROR:
    printf("At line number: %d, char number %d %s", p->line_num, p->char_num, err_lookup_table[node.value.err_code]);
    break;
  }
}

static void
print_json_node(const struct json_parser *p, const struct json_ast_node node)
{
  print_json_node_helper(p, node, 1);
}

// Reads entire file into a dynamically allocated buffer.
// Returns pointer (caller must free) and sets out_len
static char *
read_file_to_string(const char *path, size_t *out_len)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen");
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(fp);
        return NULL;
    }

    long len = ftell(fp);
    if (len < 0) {
        perror("ftell");
        fclose(fp);
        return NULL;
    }

    rewind(fp);

    char *buffer = malloc(len + 1);
    if (!buffer) {
        perror("malloc");
        fclose(fp);
        return NULL;
    }

    if (fread(buffer, 1, len, fp) != (size_t)len) {
        perror("fread");
        free(buffer);
        fclose(fp);
        return NULL;
    }

    buffer[len] = '\0';
    fclose(fp);

    if (out_len) *out_len = (size_t)len;
    return buffer;
}

static void
process_directory(const char *dirpath)
{
    DIR *dir = opendir(dirpath);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char fullpath[4096];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0) {
            perror("stat");
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            size_t length = 0;
            unsigned char *content = (unsigned char *)read_file_to_string(fullpath, &length);
            unsigned char *begin = content;
            if (content) {
                struct json_parser *p = make_parser_from_string(&content, length, lib_allocator);
                parse_json(p);
                
                if (entry->d_name[0] == 'y') {
                  printf("%s : %s\n", fullpath, p->json_node.type != JSON_ERROR ? "SUCCESS" : "FAIL");
                  print_json_node(p, p->json_node);
                  printf("\n"); 
                } else if (entry->d_name[0] == 'n') {
                  printf("%s : %s\n", fullpath, p->json_node.type != JSON_ERROR ? "FAIL" : "SUCCESS");
                  print_json_node(p, p->json_node);
                  printf("\n");
                } else {
                  printf("%s : %s\n", fullpath, "SUCCESS");
                  print_json_node(p, p->json_node);
                  printf("\n");
                }

                free_json_node(p->json_node, p);
                parser_free(p, p->source.ctx);
                parser_free(p, (void *)p);
                free(begin);
            }
        }
    }

    closedir(dir);
}

int 
main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  process_directory("test_files");

  return 0;
}

#endif
