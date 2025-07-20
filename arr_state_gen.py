#!/bin/python3
from collections import defaultdict

states = [
    "ARR_START",
    "ARR_BRACKET_START",
    "ARR_VALUE",
    "ARR_COMMA",
    "ARR_BRACKET_END",
    "ARR_ERR_TRAILING_COMMA",
    "ARR_ERR_INVALID_VALUE",
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
    ("ARR_START", [(asc('['), 'ARR_BRACKET_START'), (all_except_for(asc('[')), 'ARR_ERR_INVALID_VALUE')]),
    ("ARR_BRACKET_START", [(one_of('{["-0123456789ntf'), 'ARR_VALUE'),
                           (all_except_for(one_of('{["-0123456789ntf')), 'ARR_ERR_INVALID_VALUE')]),
    ("ARR_VALUE", [(asc(','), 'ARR_COMMA'),
                   (asc(']'), 'ARR_BRACKET_END'),
                   (all_except_for(one_of(',]')), 'ARR_ERR_TRAILING_COMMA')]),
    ("ARR_COMMA", [(one_of('{["-0123456789ntf'), 'ARR_VALUE'),
                   (all_except_for(one_of('{["-0123456789ntf')), 'ARR_ERR_INVALID_VALUE')]),
    ("ARR_BRACKET_END", [(ALL, 'ARR_BRACKET_END')]),
    ("ARR_ERR_TRAILING_COMMA", [(ALL, 'ARR_ERR_TRAILING_COMMA')]),
    ("ARR_ERR_INVALID_VALUE", [(ALL, 'ARR_ERR_INVALID_VALUE')]),
]

init_list = ''
for state_from, tran_to in transitions:
    line = '[' + state_from + '] = { \n'
    for t_list, state_to in tran_to:
        line = line + ',\n'.join([f"[{t_list[i]}] = {state_to}" for i in range(len(t_list))]) + ',\n'
    line = line + '},\n'
    init_list = init_list + line

print(init_list)
