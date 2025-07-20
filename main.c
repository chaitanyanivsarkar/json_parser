#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#define s8(str) (s8){ .val=str, .len=(sizeof(str)-1) }
#define ARRAY_LEN(arr) sizeof(arr)/sizeof(*arr)

#define vec_append(vec, val) do {                                       \
        if ((vec)->len >= (vec)->cap) {                                 \
            (vec)->cap = (vec)->cap ? 2*(vec)->cap : 16;                \
            (vec)->arr =                                                \
                realloc((vec)->arr, (vec)->cap*sizeof(*(vec)->arr));    \
        }                                                               \
        (vec)->arr[(vec)->len++] = val;                                 \
    } while(0)

typedef struct {
    const char *val;
    ptrdiff_t len;
} s8;

typedef enum {
    JSON_STRING,
    JSON_NUMBER,
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_BOOL,
    JSON_NULL
} Json_Tag;

struct json_type;
typedef struct json_type Json_Type;

struct json_arr {
    Json_Type *arr;
    ptrdiff_t len;
};

struct json_pair {
    char *key;
    Json_Type *type;
};

struct json_object {
    ptrdiff_t len;
    struct json_pair *arr;
};

struct json_type {
    Json_Tag tag;
    union {
        struct json_arr arr; /* Json_Array([Json_Type]) */
        struct json_object obj; /* Json_Object([(String, Json_Type)]) */
    } schema;
};

typedef struct {} Property_Map;
typedef struct {
    Json_Type schema;
    ptrdiff_t lineno;
    ptrdiff_t cursor;
    char err[256];
    enum Parser_State { JSON_PARSER_DONE, JSON_PARSER_ERROR, JSON_PARSER_OK } state;
    s8 json_string;
    Property_Map m;
} Json_Parser;

struct json_ast_node;
typedef struct json_ast_node Json_AST_Node;

struct json_ast_obj_pair {
    s8 key;
    struct json_ast_node *val;
};

struct vec_json_obj_pair {
    struct json_ast_obj_pair *arr;
    ptrdiff_t len;
    ptrdiff_t cap;
};

struct vec_json_ast_node {
    struct json_ast_node *arr;
    ptrdiff_t len;
    ptrdiff_t cap;
};

struct json_ast_node {
    Json_Tag type;
    union {
        double number;
        bool b;
        s8 string;
        struct vec_json_obj_pair fields;
        struct vec_json_ast_node arr;
    } value;
};

Json_AST_Node * parse_json_value(char **json_string);

Json_AST_Node * parse_number(char **json_string) {
    enum number_state{
        ERROR = 0,
        START = 1,
        NEGATIVE,
        ZERO,
        DIGIT_1_9,
        DIGIT,
        DECIMAL_POINT,
        DECIMAL_DIGIT,
        EXPONENT,
        EXPONENT_POSITIVE,
        EXPONENT_NEGATIVE,
        EXPONENT_DIGIT,
        END
    };

    enum number_state state = START;
    enum number_state jump_table[END][128] = {
#include "number_state_table.txt"
    };
    enum number_state next_state = START;
    long long int number = 0;
    long long int decimal = 0;
    int decimal_len = 0;
    long long int exponent = 0;
    int sign = 1;
    int exponent_sign = 1;
    while (state != END && state != ERROR && **json_string) {
        next_state = jump_table[state][(unsigned char)**json_string];
        switch(next_state) {
            case START: break;
            case ZERO: break;
            case DECIMAL_POINT: break;
            case EXPONENT: break;
            case EXPONENT_POSITIVE: break;
            case END: break;
            case ERROR: break;
            case NEGATIVE: {
                sign = -1;
                break;
            }
            case DIGIT_1_9: case DIGIT: {
                number = number*10 + (long long int)(**json_string - '0');
                break;
            }
            case DECIMAL_DIGIT: {
                decimal = decimal*10 + (long long int)(**json_string - '0');
                ++decimal_len;
                break;
            }
            case EXPONENT_NEGATIVE: {
                exponent_sign = -1;
                break;
            }
            case EXPONENT_DIGIT: {
                exponent = exponent*10 + (long long int)(**json_string - '0');
                break;
            }
        }
        if (next_state == ERROR) {
            switch (state) {
                case START: {
                    fprintf(stderr, "Expected - or 0-9 got %c\n", **json_string);
                    break;
                }
                case NEGATIVE: {
                    fprintf(stderr, "Expected digits 0-9 got %c\n", **json_string);
                    break;
                }
                case ZERO: {
                    fprintf(stderr, "Expected end/./e got %c\n", **json_string);
                    break;
                }
                case DIGIT_1_9: {
                    fprintf(stderr, "Expected digits, ., e, or end got %c\n", **json_string);
                    break;
                }
                case DIGIT: {
                    fprintf(stderr, "Expected digits, ., e, or end got %c\n", **json_string);
                    break;
                }
                case DECIMAL_POINT: {
                    fprintf(stderr, "Expected digits got %c\n", **json_string);
                    break;
                }
                case DECIMAL_DIGIT: {
                    fprintf(stderr, "Expected digits, e, or end got %c\n", **json_string);
                    break;
                }
                case EXPONENT: {
                    fprintf(stderr, "Expected +/- or digits got %c\n", **json_string);
                    break;
                }
                case EXPONENT_POSITIVE: {
                    fprintf(stderr, "Expected digits got %c\n", **json_string);
                    break;
                }
                case EXPONENT_NEGATIVE: {
                    fprintf(stderr, "Expected digits got %c\n", **json_string);
                    break;
                }
                case EXPONENT_DIGIT: {
                    fprintf(stderr, "Expected digits, or end got %c\n", **json_string);
                    break;
                }
                default: {
                    fprintf(stderr, "Someone messed up big time if we got here %c\n", **json_string);
                    break;
                }
            }
            return NULL;
        }
        ++(*json_string);
        state = next_state;
    }
    if (state == NEGATIVE || state == EXPONENT_POSITIVE || state == EXPONENT_NEGATIVE || state == DECIMAL_POINT) {
        fprintf(stderr, "Number ended early\n");
        return NULL;
    }
    double result = (double)number;
    if (decimal_len > 0) {
        result += (double)decimal/pow(10.0, decimal_len);
    }
    if (exponent != 0) {
        result *= pow(10.0, exponent_sign*exponent);
    }
    result *= sign;

    struct json_ast_node *num_node = malloc(sizeof(struct json_ast_node));
    num_node->type = JSON_NUMBER;
    num_node->value.number = result;
    return num_node;
}

void log_error_from_string_state(int state) {
    (void)state;
    return;
}

typedef struct {
    char *arr;
    ptrdiff_t len;
    ptrdiff_t cap;
} String_Builder;

Json_AST_Node * parse_string(char **json_string) {
    (void)json_string;
  
  
}

Json_AST_Node * parse_object(char **json_string) {
    ++(*json_string);
    
}

Json_AST_Node * parse_array(char **json_string) {
    ++(*json_string);
    Json_AST_Node *node = malloc(sizeof(Json_AST_Node));
    node->type = JSON_ARRAY;
    while (**json_string != ']') {
        vec_append(&(node->value.arr), parse_json_value(json_string));
    }

    return node;
}

Json_AST_Node * parse_json_value(char **json_string) {
    //skip_spaces(json_string);
    switch (**json_string) {
        case '{': return parse_object(&json_string);
        case '[': return NULL;// parse_array(&json_string);
        case '"': return parse_string(&json_string);
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case '-': return parse_number(json_string);
        default: {
            fprintf(stderr, "Found %c instead of the expected json values\n", **json_string);
            return NULL;
        }
    }
}

Json_Parser *json_parse(const Json_Type *schema, char *json_string) {
    (void)schema;
    (void)json_string;
    return NULL;
}

Json_AST_Node * json_get_field(Json_Parser *parser, s8 property) {
    (void)parser;
    (void)property;
    return NULL;
}
