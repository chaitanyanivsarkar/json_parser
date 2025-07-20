#!/bin/python3
from collections import defaultdict

states = [
    'OBJ_START',
    'OBJ_CURLY_START',
    'OBJ_KEY',
    'OBJ_KV_SEPARATOR',
    'OBJ_VALUE',
    'OBJ_COMMA',
    'OBJ_CURLY_END',
    'OBJ_ERR_OBJ_CURLY_START',
    'OBJ_ERR_KEY_NOT_STRING',
    'OBJ_ERR_INVALID_OBJ_VALUE',
    'OBJ_ERR_MULTIPLE_COLON',
    'OBJ_ERR_COLON_NOT_FOUND',
    'OBJ_ERR_VALUE_END',
    'OBJ_ERR_TRAILING_COMMA',
]

ALL = [c for c in range(256)]
def all_except_for(l):
    return [c for c in range(256) if c not in l]

END_CHARS = [ord('}'), ord(']'), ord(','), ord(' '), ord('\t'), ord('\r'), ord('\n')]

def asc(c):
    return [ord(c)]

def one_of(l):
    return [ord(c) for c in l]

transitions = [
    ('OBJ_START', [(asc('{'), 'OBJ_CURLY_START'),
                   (all_except_for(asc('{')), 'OBJ_ERR_OBJ_CURLY_START')]),
    ('OBJ_CURLY_START', [(asc('"'), 'OBJ_KEY'),
                         (asc('}'), 'OBJ_CURLY_END'),
                         (all_except_for(one_of('"}')), 'OBJ_ERR_KEY_NOT_STRING')]),
    ('OBJ_KEY', [(asc(':'), 'OBJ_KV_SEPARATOR'),
                 (all_except_for(asc(':')), 'OBJ_ERR_COLON_NOT_FOUND')]),
    ('OBJ_KV_SEPARATOR', [(one_of('{["-0123456789ntf'), 'OBJ_VALUE'),
                          (asc(':'), 'OBJ_ERR_MULTIPLE_COLON'),
                          (all_except_for(one_of('{["-0123456789ntf:')), 'OBJ_ERR_INVALID_OBJ_VALUE')]),
    ('OBJ_VALUE', [(asc(','), 'OBJ_COMMA'),
                   (asc('}'), 'OBJ_CURLY_END'),
                   (all_except_for(one_of(',}')), 'OBJ_ERR_VALUE_END')]),
    ('OBJ_COMMA', [(asc('"'), 'OBJ_KEY'),
                   (all_except_for(asc('"')), 'OBJ_ERR_TRAILING_COMMA')]),
    ('OBJ_CURLY_END', [(ALL, 'OBJ_CURLY_END')]),
    ('OBJ_ERR_OBJ_CURLY_START', [(ALL, 'OBJ_ERR_OBJ_CURLY_START')]),
    ('OBJ_ERR_KEY_NOT_STRING', [(ALL, 'OBJ_ERR_KEY_NOT_STRING')]),
    ('OBJ_ERR_INVALID_OBJ_VALUE', [(ALL, 'OBJ_ERR_INVALID_OBJ_VALUE')]),
    ('OBJ_ERR_MULTIPLE_COLON', [(ALL, 'OBJ_ERR_MULTIPLE_COLON')]),
    ('OBJ_ERR_TRAILING_COMMA', [(ALL, 'OBJ_ERR_TRAILING_COMMA')]),
]

init_list = ''
for state_from, tran_to in transitions:
    line = '[' + state_from + '] = { \n'
    for t_list, state_to in tran_to:
        line = line + ',\n'.join([f"[{t_list[i]}] = {state_to}" for i in range(len(t_list))]) + ',\n'
    line = line + '},\n'
    init_list = init_list + line

print(init_list)
