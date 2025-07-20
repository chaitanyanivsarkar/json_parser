#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <float.h>

typedef struct {
  ptrdiff_t len;
  ptrdiff_t cap;
  unsigned char *str;
} String_Builder;

void sb_append_char(String_Builder *sb, unsigned char c) {
  if (sb->len >= sb->cap -1) {
    sb->cap = sb->cap == 0 ? 16 : 2*sb->cap;
    sb->str = realloc(sb->str, sb->cap);
    memset(sb->str + sb->len, 0, sb->cap - sb->len);
  }
  sb->str[sb->len++] = c;
}

unsigned char * sb_tostr(String_Builder *sb) {
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

void sb_append_utf8(String_Builder *sb, utf8 u8) {
  if (u8.b1 < 128) {
    sb_append_char(sb, u8.b1);
  } else if (u8.b1 >= 0b11000000 && u8.b1 <= 0b11011111) {
    sb_append_char(sb, u8.b1);
    sb_append_char(sb, u8.b2);
  } else if (u8.b1 >= 0b11100000 && u8.b1 <= 0b11101111) {
    sb_append_char(sb, u8.b1);
    sb_append_char(sb, u8.b2);
    sb_append_char(sb, u8.b3);
  } else if (u8.b1 >= 0b11110000 && u8.b1 <= 0b11110111) {
    sb_append_char(sb, u8.b1);
    sb_append_char(sb, u8.b2);
    sb_append_char(sb, u8.b3);
    sb_append_char(sb, u8.b4);
  } else {
    fprintf(stderr, "The state machine will never come here so if you are seeing this you copied this function without modifying for your usecase asshole\n");
    assert(false);
  }
}

int convert_to_hex_digit(unsigned char h) {
  if (h <= '0' && h >= '9') {
    return h - '0';
  } else if (h >= 'A' && h <= 'F') {
    return 0xA + (h - 'A');
  } else if (h >= 'a' && h <= 'f') {
    return 0xA + (h - 'a');
  } else {
    fprintf(stderr, "Dipshit first learn what a hexadecimal digit is then use this function\n");
    return -1;
  }
}

void ub_append_hex(utf16_builder *ub, unsigned char h) {
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
void ub_append_lshex(utf16_builder *ub, unsigned char h) {
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
utf8 ub_toutf8(utf16_builder ub) {
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
} Num_Builder;

Num_Builder nb_negative(Num_Builder nb) {
  nb.sign = -1;
  return nb;
}
Num_Builder nb_negative_exp(Num_Builder nb) {
  nb.expsign = -1;
  return nb;
}
Num_Builder nb_append_exp(Num_Builder nb, char e) {
  assert(e <= '9' && e >= '0');
  if (nb.exponent <= (INT_MAX - (e - '0'))/10) nb.exponent = nb.exponent * 10 + (e - '0');
  else nb.exponent = INT_MAX;

  return nb;
}
Num_Builder nb_append_decimal(Num_Builder nb, char d) {
  assert(d <= '9' && d >= '0');
  if (nb.decimal <= (INT_MAX - (d - '0'))/10) {
    nb.decimal = nb.decimal * 10 + (d - '0');
    nb.ndecdigits += 1;
  }
  else nb.decimal = INT_MAX;
  return nb;
}
Num_Builder nb_append_int(Num_Builder nb, char i) {
  assert(i <= '9' && i >= '0');
  if (nb.integer <= (INT_MAX - (i - '0'))/10) nb.integer = nb.integer * 10 + (i - '0');
  else nb.integer = INT_MAX;
  return nb;
}

double nb_todouble(Num_Builder nb) {
  if (nb.integer == INT_MAX || nb.decimal == INT_MAX) return 0.0;
  if (nb.exponent > 308) return 0.0;
  
  if (nb.expsign == -1
      && DBL_MIN*pow(10, nb.exponent) > nb.integer + (nb.decimal/pow(10, nb.ndecdigits))) return 0.0;
  if (nb.expsign == 1
      && DBL_MAX/pow(10, nb.exponent) < nb.integer + (nb.decimal/pow(10, nb.ndecdigits))) return 0.0;
  
  double result = (1.0*(nb.integer * nb.sign)
                   + ((double)nb.decimal)/pow(10, nb.ndecdigits))
    * pow(10, nb.exponent * nb.expsign);
  
  return result;
}

struct json_ast_node {
  enum json_type {
    JSON_NULL, JSON_BOOL,
    JSON_ARRAY, JSON_OBJECT,
    JSON_NUMBER, JSON_STRING,
    JSON_ERROR
  } type;

  union {
    bool b;
    double num;
    unsigned char *s;
    struct json_arr {
      struct json_ast_node **arr;
      ptrdiff_t len;
      ptrdiff_t cap;
    } vec;
    struct json_fields {
      unsigned char **keys;
      struct json_ast_node **vals;
      ptrdiff_t len;
      ptrdiff_t cap; 
    } obj;
  } value;
  char err[128];
};

void json_vec_append(struct json_arr *arr, struct json_ast_node *node) {
  if (arr->cap <= arr->len) {
    arr->cap = arr->cap == 0 ? 16 : 2*(arr->cap);
    arr->arr = realloc(arr->arr, sizeof(node)*(arr->cap));
    if (arr->arr == NULL) exit(1);
  }
  arr->arr[arr->len++] = node;
}

void json_obj_append(struct json_fields *obj, unsigned char *key, struct json_ast_node *val) {
  if (obj->cap <= obj->len) {
    obj->cap = obj->cap == 0 ? 4 : 2*(obj->cap);
    obj->keys = realloc(obj->keys, sizeof(key)*(obj->cap));
    obj->vals = realloc(obj->vals, sizeof(val)*(obj->cap));
    if (!(obj->keys && obj->vals)) exit(1);
  }
  obj->keys[obj->len] = key;
  obj->vals[obj->len] = val;
  obj->len++;
}

static bool is_ws(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }

static void
skip_whitespace(unsigned char *json_string[1])
{
  while(**json_string && is_ws(**json_string)) ++(*json_string);
}
static struct json_ast_node * parse_base_value(unsigned char *json_string[static 1]);
static struct json_ast_node * parse_array(unsigned char *json_string[static 1]);
static struct json_ast_node * parse_object(unsigned char *json_string[static 1]);
static struct json_ast_node * make_json_error(char err[]);

extern struct json_ast_node *
parse_json_value(unsigned char **json_string)
{
  skip_whitespace(json_string);
  switch(**json_string) {
    case '{': return parse_object(json_string);
    case '[': return parse_array(json_string);
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
    case 'f': return parse_base_value(json_string);
    default: return make_json_error("unexpected value found\n");
  }
}

static struct json_ast_node *
make_json_empty_object(void)
{
  struct json_ast_node *node = malloc(sizeof(struct json_ast_node));
  memset(node, 0, sizeof(struct json_ast_node));
  node->type = JSON_OBJECT;
  return node;
}

static struct json_ast_node *
make_json_empty_array(void)
{
  struct json_ast_node *node = malloc(sizeof(struct json_ast_node));
  memset(node, 0, sizeof(struct json_ast_node));
  node->type = JSON_ARRAY;
  return node;
}

static void
free_json_node(struct json_ast_node *node)
{
  switch (node->type) {
  case JSON_NUMBER:
  case JSON_BOOL: 
  case JSON_NULL: { free(node); return; }
  case JSON_STRING:
    if (node->value.s)
      free((void *)node->value.s);
    free(node);
    return;
  case JSON_OBJECT: {
    for (ptrdiff_t i = 0; i < node->value.obj.len; ++i) {
      free(node->value.obj.keys[i]);
      free_json_node(node->value.obj.vals[i]);
    }
    free(node->value.obj.keys);
    free(node->value.obj.vals);
    free(node);
    return;
  }
  case JSON_ARRAY: {
    for (ptrdiff_t i = 0; i < node->value.vec.len; ++i) {
      free_json_node(node->value.vec.arr[i]);
    }
    free(node->value.vec.arr);
    free(node);
    return;
  }
  case JSON_ERROR: {
    free(node);
    return;
  }
  }
}

unsigned char *
move_ustr(struct json_ast_node *str_node)
{
  assert(str_node->type == JSON_STRING);
  unsigned char *res = str_node->value.s;
  str_node->value.s = NULL;
  free_json_node(str_node);

  return res;
}

enum json_obj_state {
    OBJ_START,
    OBJ_CURLY_START,
    OBJ_KEY,
    OBJ_KV_SEPARATOR,
    OBJ_VALUE,
    OBJ_COMMA,
    OBJ_CURLY_END,
    OBJ_ERR_OBJ_CURLY_START,
    OBJ_ERR_KEY_NOT_STRING,
    OBJ_ERR_INVALID_OBJ_VALUE,
    OBJ_ERR_MULTIPLE_COLON,
    OBJ_ERR_COLON_NOT_FOUND,
    OBJ_ERR_VALUE_END,
    OBJ_ERR_TRAILING_COMMA,
    OBJ_STATE_SIZE,
};
static enum json_obj_state obj_transition_table[OBJ_STATE_SIZE][256] = {
  #include "obj_transition.table"
};

static struct json_ast_node *
parse_object(unsigned char *json_string[static 1])
{
  enum json_obj_state state = OBJ_START;
  struct json_ast_node *obj_node = make_json_empty_object();
  struct json_ast_node *key = NULL;
  struct json_ast_node *val = NULL;
  char err[128] = {0};
  
  do {
    skip_whitespace(json_string);
    state = obj_transition_table[state][(unsigned char)**json_string];
    switch (state) {
    case OBJ_START:
      //printf("object start\n");
      break;
    case OBJ_CURLY_START:
      // printf("found opening curly bracket\n");
      break;
    case OBJ_KEY:
      //printf("parsing the key\n");
      key = parse_json_value(json_string);
      if (key->type != JSON_STRING) {
        //printf("Key is not a valid string");
        state = OBJ_ERR_KEY_NOT_STRING;
      }
      //printf("successfully parsed the key\n");
      break;
    case OBJ_KV_SEPARATOR:
      //printf("found colon\n");
      break;
    case OBJ_VALUE:
      //printf("parsing the value\n");
      val = parse_json_value(json_string);
      if (val->type == JSON_ERROR) {
        state = OBJ_ERR_INVALID_OBJ_VALUE;
        strncpy(err, val->err, 128);
        free_json_node(key);
        free_json_node(val);
      } else {
        json_obj_append(&(obj_node->value.obj), move_ustr(key), val);
      }
      break;
    case OBJ_COMMA:
      //printf("found comma\n");
      key = NULL;
      val = NULL;
      break;
    case OBJ_CURLY_END:
      return obj_node;
      break;
    case OBJ_STATE_SIZE: break;
    case OBJ_ERR_OBJ_CURLY_START:
      free_json_node(obj_node);
      return make_json_error("Json Objects must start with a opening curly bracket");
    case OBJ_ERR_KEY_NOT_STRING:
      free_json_node(obj_node);
      return make_json_error("Json Object keys must be valid json strings");
    case OBJ_ERR_INVALID_OBJ_VALUE:
      free_json_node(obj_node);
      return make_json_error(err);
    case OBJ_ERR_MULTIPLE_COLON:
      free_json_node(obj_node);
      return make_json_error("Key value pair must be separated by only one :");
    case OBJ_ERR_COLON_NOT_FOUND:
      //printf("colon not found \n");
      free_json_node(obj_node);
      return make_json_error("Json object key and value should be separated by a colon ':'");
    case OBJ_ERR_VALUE_END:
      free_json_node(obj_node);
      return make_json_error("Json key value pairs can only be succeded by a comma or a curly closing bracket");
    case OBJ_ERR_TRAILING_COMMA:
      free_json_node(obj_node);
      return make_json_error("Trailing commas are not allowed in Json Objects");
    }

    if (val && val->type == JSON_NUMBER) {
      if (**json_string != ',' && **json_string != '}' && **json_string != ']') {
        ++(*json_string);
      }
    } else {
      ++(*json_string);
    }
  } while (**json_string && state != OBJ_CURLY_END);

  return NULL;
}

enum json_arr_state {
  ARR_START,
  ARR_BRACKET_START,
  ARR_VALUE,
  ARR_COMMA,
  ARR_BRACKET_END,
  ARR_ERR_TRAILING_COMMA,
  ARR_ERR_INVALID_VALUE,
  ARR_STATE_SIZE,
};

static enum json_arr_state arr_transition_table[ARR_STATE_SIZE][256] = {
  #include "arr_transition.table"
};
  
static struct json_ast_node *
parse_array(unsigned char *json_string[static 1])
{
  enum json_arr_state state = ARR_START;
  struct json_ast_node *arr_node = make_json_empty_array();
  struct json_ast_node *node = NULL;
  char err[128];
  do {
    state = arr_transition_table[state][(unsigned char)**json_string];
    switch (state) {
    case ARR_START: break;
    case ARR_BRACKET_START: break;

    case ARR_VALUE:
      node = parse_json_value(json_string);
      if (node->type == JSON_ERROR) {
        state = ARR_ERR_INVALID_VALUE;
        strncpy(err, node->err, 128);
      }
      json_vec_append(&(arr_node->value.vec), node);
      break;

    case ARR_COMMA:
      node = NULL;
      break;
    case ARR_BRACKET_END:
      return arr_node;

    case ARR_ERR_INVALID_VALUE:
      free_json_node(arr_node);
      return make_json_error(err);
    case ARR_ERR_TRAILING_COMMA:
      free_json_node(arr_node);
      return make_json_error("Trailing commas are not allowed in JSON arrays");
    case ARR_STATE_SIZE: 
      fprintf(stderr, "This state should also be unreachable help!!\n");
      free_json_node(arr_node);
      return make_json_error("Invalid State transition occured please go and slap the developer");
    }
    
    if (node && node->type == JSON_NUMBER) {
      if (**json_string != ',' && **json_string != '}' && **json_string != ']') {
        ++(*json_string);
      }
    } else {
      ++(*json_string);
    }
    skip_whitespace(json_string);    
  } while (**json_string);

  return NULL;
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

static struct json_ast_node *
make_json_null()
{
  struct json_ast_node *node = malloc(sizeof(struct json_ast_node));
  node->type = JSON_NULL;
  return node;
}

static struct json_ast_node *
make_json_number(double num)
{
  struct json_ast_node *node = malloc(sizeof(struct json_ast_node));
  node->type = JSON_NUMBER;
  node->value.num = num;
  return node;
}

static struct json_ast_node *
make_json_bool(bool b)
{
  struct json_ast_node *node = malloc(sizeof(struct json_ast_node));
  node->type = JSON_BOOL;
  node->value.b = b;
  return node;
}

static struct json_ast_node *
make_json_error(char err[])
{
  struct json_ast_node *node = malloc(sizeof(struct json_ast_node));
  node->type = JSON_ERROR;
  strncpy(node->err, err, 128);

  return node;
}

static struct json_ast_node *
make_json_string(unsigned char *str)
{
  struct json_ast_node *node = malloc(sizeof(struct json_ast_node));
  node->type = JSON_STRING;
  node->value.s = str;
  return node;
}

static struct json_ast_node *
parse_base_value(unsigned char *json_string[static 1])
{
  enum json_bv_state current_state = START;
  String_Builder sb = {0};
  Num_Builder nb = {1, 1, 0, 0, 0, 0};
  utf16_builder ub = {0};
  
  do {
    current_state = transition_table[current_state][(unsigned char)**json_string];
    // populate the AST node here
    switch (current_state) {
    case START: break;
    case ERR_INVALID_START:
      return make_json_error("Invalid starting character");
    case NULL_N: break;
    case NULL_U: break;
    case NULL_L1: break;
    case NULL_L2:
      return make_json_null();
      
    case ERR_EXPECTED_NULL: return make_json_error("Expected null");
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
      return make_json_error("Expected true");
    case ERR_EXPECTED_FALSE:
      return make_json_error("Expected false");

    case STR_QUOTE_BEGIN: break;

    case STR_UTF_1BYTE:
      sb_append_char(&sb, **json_string);
      break; 
    case STR_UTF_2BYTE_1:
      sb_append_char(&sb, **json_string);
      break;
    case STR_UTF_2BYTE_2:
      sb_append_char(&sb, **json_string);
      break;
    case STR_UTF_3BYTE_1:
      sb_append_char(&sb, **json_string);
      break;
    case STR_UTF_3BYTE_2:
      sb_append_char(&sb, **json_string);
      break;
    case STR_UTF_3BYTE_3:
      sb_append_char(&sb, **json_string);
      break;
    case STR_UTF_4BYTE_1:
      sb_append_char(&sb, **json_string);
      break;
    case STR_UTF_4BYTE_2:
      sb_append_char(&sb, **json_string);
      break;
    case STR_UTF_4BYTE_3:
      sb_append_char(&sb, **json_string);
      break;
    case STR_UTF_4BYTE_4:
      sb_append_char(&sb, **json_string);
      break;

    case ERR_INVALID_UTF8:
      return make_json_error("Invalid UTF-8 encountered");

    case STR_CONTROL_SLASH: break;
    case STR_CONTROL_QUOTE:
      sb_append_char(&sb, '"');
      break;
    case STR_CONTROL_REVSOL:
      sb_append_char(&sb, '\\');
      break; 
    case STR_CONTROL_SOL:
      sb_append_char(&sb, '/');
      break; 
    case STR_CONTROL_B:
      sb_append_char(&sb, '\b');
      break; 
    case STR_CONTROL_F:
      sb_append_char(&sb, '\f');
      break; 
    case STR_CONTROL_N:
      sb_append_char(&sb, '\n');
      break; 
    case STR_CONTROL_R:
      sb_append_char(&sb, '\r');
      break; 
    case STR_CONTROL_T:
      sb_append_char(&sb, '\t');
      break; 

    case ERR_INVALID_ESCAPE_CHAR:
      return make_json_error("Invalid escape character found");

    case STR_CONTROL_U: break;
    case STR_UNICODE_HEX1:
      ub_append_hex(&ub, **json_string);
      break; 
    case STR_UNICODE_HEX2:
      ub_append_hex(&ub, **json_string);
      break; 
    case STR_UNICODE_HEX3:
      ub_append_hex(&ub, **json_string);
      break; 
    case STR_UNICODE_HEX4:
      ub_append_hex(&ub, **json_string);
      sb_append_utf8(&sb, ub_toutf8(ub));
      break; 
    case STR_UNICODE_MAYBE_HS_HEX1:
      ub_append_hex(&ub, **json_string);
      break; 
    case STR_UNICODE_HS_HEX2:
      ub_append_hex(&ub, **json_string);
      break; 
    case STR_UNICODE_HS_HEX3:
      ub_append_hex(&ub, **json_string);
      break; 
    case STR_UNICODE_HS_HEX4:
      ub_append_hex(&ub, **json_string);
      break; 
    case STR_UNICODE_LS_CONTROL_SLASH: break;
    case STR_UNICODE_LS_CONTROL_U: break;
    case STR_UNICODE_LS_HEX1:
      ub_append_lshex(&ub, **json_string);
      break; 
    case STR_UNICODE_LS_HEX2:
      ub_append_lshex(&ub, **json_string);
      break; 
    case STR_UNICODE_LS_HEX3:
      ub_append_lshex(&ub, **json_string);
      break; 
    case STR_UNICODE_LS_HEX4:
      ub_append_lshex(&ub, **json_string);
      sb_append_utf8(&sb, ub_toutf8(ub));
      break; 

    case ERR_INVALID_UNICODE_ESCAPE:
      return make_json_error("Invalid UTF-16 found");
    case ERR_LONE_SURROGATE:
      return make_json_error("Lone surrogate found in UTF-16");
    case ERR_UNPAIRED_SURROGATE:
      return make_json_error("Unpaired Surrogate found");

    case STR_QUOTE_END:
      return make_json_string(sb_tostr(&sb));

    case NUM_MINUS:
      nb = nb_negative(nb);
      break; 
    case NUM_ZERO:
      nb = nb_append_int(nb, **json_string);
      break; 
    case NUM_DIGIT19:
      nb = nb_append_int(nb, **json_string);
      break; 
    case NUM_DIGIT:
      nb = nb_append_int(nb, **json_string);
      break; 

    case NUM_DECIMAL: break;
    case NUM_DECIMAL_DIGIT:
      nb = nb_append_decimal(nb, **json_string);
      break; 

    case NUM_EXPONENT: break;
    case NUM_EXP_PLUS: break;
    case NUM_EXP_MINUS:
      nb = nb_negative_exp(nb);
      break; 
    case NUM_EXP_DIGIT:
      nb = nb_append_exp(nb, **json_string);
      break; 

    case ERR_LEADING_ZEROES:
      return make_json_error("Leading zeroes are not allowed in numbers");
    case ERR_PLUS_SIGN:
      return make_json_error("Leading + is not allowed in numbers");
    case ERR_NO_DIGIT_BEFORE_DECIMAL:
      return make_json_error("Expected atleast 1 digit before decimal");
    case ERR_NO_DIGIT_BEFORE_EXPONENT:
      return make_json_error("Expected atleast 1 digit before exponent");
    case ERR_NOT_A_NUMBER:
      return make_json_error("Malformed number");
    case ERR_DOUBLE_DECIMAL:
      return make_json_error("Found more than 1 decimal points");
    case ERR_DOUBLE_EXPONENT:
      return make_json_error("Found more than 1 exponent");
    case NUM_END:
      return make_json_number(nb_todouble(nb));
    case JSON_BV_STATE_SIZE:
      return make_json_error("Someone fucked up");
    }
    ++(*json_string);
  } while (**json_string);

  // To handle the case where the json string is just a number since numbers don't have a fixed ending character
  return make_json_number(nb_todouble(nb));
}

extern bool ustreq(const unsigned char a[static 1], const unsigned char b[static 1]) {
  while(*a && *b) {
    if (*a != *b) return false;
  }
  if (!(*a)) return *b == '\0';
  if (!(*b)) return *a == '\0';

  return true;
}

void
print_type(const struct json_ast_node node[static 1])
{
  char *lookup_table[] = {
    [JSON_NULL] = "JSON_NULL",
    [JSON_BOOL] = "JSON_BOOL",
    [JSON_ARRAY] = "JSON_ARRAY",
    [JSON_OBJECT] = "JSON_OBJECT",
    [JSON_NUMBER] = "JSON_NUMBER",
    [JSON_STRING] = "JSON_STRING",
    [JSON_ERROR] = "JSON_ERROR"
  };

  printf("%s\n", lookup_table[node->type]);
}

void
print_json_node_helper(const struct json_ast_node node[static 1], int level)
{
  switch (node->type) {
  case JSON_NULL:
    printf("null");
    break;
  case JSON_BOOL:
    printf("%s", (node->value.b ? "true" : "false"));
    break;
  case JSON_ARRAY:
    printf("[\n");
    for (int i = 0; i < level; ++i) printf("\t");
    if (node->value.vec.len) {
      print_json_node_helper(node->value.vec.arr[0], level+1);
    }

    for (ptrdiff_t i = 1; i < node->value.vec.len; ++i) {
      printf(",\n");
      for (int i = 0; i < level; ++i) printf("\t");
      print_json_node_helper(node->value.vec.arr[i], level+1);
    }
    printf("\n");
    for (int i = 0; i < level-1; ++i) printf("\t");
    printf("]");
    break;
  case JSON_OBJECT:
    printf("{\n");
    for (int i = 0; i < level; ++i) printf("\t");
    if (node->value.obj.len) {
      printf("\"%s\"", (char *)node->value.obj.keys[0]);
      printf(" : ");
      print_json_node_helper(node->value.obj.vals[0], level+1);
    }

    for(ptrdiff_t i = 1; i < node->value.obj.len; ++i) {
      printf(",\n");
      for (int i = 0; i < level; ++i) printf("\t");
      printf("\"%s\"", (char *)node->value.obj.keys[i]);
      printf(" : ");
      print_json_node_helper(node->value.obj.vals[i], level+1);
    }
    printf("\n");
    for (int i = 0; i < level-1; ++i) printf("\t");
    printf("}");
    break;
  case JSON_NUMBER:
    printf("%lf", node->value.num);
    break;
  case JSON_STRING:
    printf("\"%s\"", (char *)node->value.s);
    break;
  case JSON_ERROR:
    printf("ERROR::%s", node->err);
    break;
  }
}

void
print_json_node(const struct json_ast_node node[static 1])
{
  print_json_node_helper(node, 1);
}

// TODO: Complete the implementation for UTF-16 to UTF-8 conversion
// TODO: Run and pass the complete json test suite and measure performance as well
// TODO: Combine the 3 state machines into 1 large state machine
// TODO: Add a top level parse_json() function that takes a json_source object and returns a parser handle
// TODO: Add helper methods to extract values from the parser
// TODO: Add line and cursor number information to the AST nodes to improve error handling
// TODO: Create a json_source ADT that can take either a string or a file or a stream
// TODO: Create a final header file along with a statically linked library compiled with -Ofast and -pgo
// TODO: Add custom allocator support and reduce the number of allocations if possible
// TODO: 

int 
main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  // Run Unit tests here
  unsigned char *s = (unsigned char *)"\"A simple json string\"";
  char *expected = "A simple json string";
  struct json_ast_node *node = parse_json_value(&s);
  assert(strcmp(expected, (char *)node->value.s) == 0);
  print_json_node(node);
  printf("\n");
  free_json_node(node);

  s = (unsigned char *)"123.46";
  node = parse_json_value(&s);
  assert(123.46 == node->value.num);
  print_json_node(node);
  printf("\n");
  free_json_node(node);

  s = (unsigned char *)"{ \"key\" : 123.3, \"key2\": [{\"key4\" : false}, true], \"key3\":\"another value\" }";
  node = parse_json_value(&s);
  print_json_node(node);
  printf("\n");
  free_json_node(node);

  // Read the file list from test_files directory
  // run the parser on all of them and verify if it is able to parse
  // i_ indicates implementation defined behaviour
  // y_ indicates valid json
  // n_ indicates invalid json
  
  return 0;
}
