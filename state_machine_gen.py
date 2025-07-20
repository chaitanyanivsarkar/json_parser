#!/bin/python3
from collections import defaultdict

states = [
    "START",
    "ERR_INVALID_START",
    "NULL_N",
    "NULL_U",
    "NULL_L1",
    "NULL_L2",
    "ERR_EXPECTED_NULL",
    "BOOL_T",
    "BOOL_R",
    "BOOL_U",
    "BOOL_TE",
    "BOOL_F",
    "BOOL_A",
    "BOOL_L",
    "BOOL_S",
    "BOOL_FE",
    "ERR_EXPECTED_TRUE",
    "ERR_EXPECTED_FALSE",
    "STR_QUOTE_BEGIN",
    "STR_UTF_1BYTE",
    "STR_UTF_2BYTE_1",
    "STR_UTF_2BYTE_2",
    "STR_UTF_3BYTE_1",
    "STR_UTF_3BYTE_2",
    "STR_UTF_3BYTE_3",
    "STR_UTF_4BYTE_1",
    "STR_UTF_4BYTE_2",
    "STR_UTF_4BYTE_3",
    "STR_UTF_4BYTE_4",
    "ERR_INVALID_UTF8",
    "STR_CONTROL_SLASH",
    "STR_CONTROL_QUOTE",
    "STR_CONTROL_REVSOL",
    "STR_CONTROL_SOL",
    "STR_CONTROL_B",
    "STR_CONTROL_F",
    "STR_CONTROL_N",
    "STR_CONTROL_R",
    "STR_CONTROL_T",
    "ERR_INVALID_ESCAPE_CHAR",
    "STR_CONTROL_U",
    "STR_UNICODE_HEX1",
    "STR_UNICODE_HEX2",
    "STR_UNICODE_HEX3",
    "STR_UNICODE_HEX4",
    "STR_UNICODE_MAYBE_HS_HEX1",
    "STR_UNICODE_HS_HEX2",
    "STR_UNICODE_HS_HEX3",
    "STR_UNICODE_HS_HEX4",
    "STR_UNICODE_LS_CONTROL_SLASH",
    "STR_UNICODE_LS_CONTROL_U",
    "STR_UNICODE_LS_HEX1",
    "STR_UNICODE_LS_HEX2",
    "STR_UNICODE_LS_HEX3",
    "STR_UNICODE_LS_HEX4",
    "ERR_INVALID_UNICODE_ESCAPE",
    "ERR_LONE_SURROGATE",
    "ERR_UNPAIRED_SURROGATE",
    "STR_QUOTE_END",
    "NUM_MINUS",
    "NUM_ZERO",
    "NUM_DIGIT19",
    "NUM_DIGIT",
    "NUM_DECIMAL",
    "NUM_DECIMAL_DIGIT",
    "NUM_EXPONENT",
    "NUM_EXP_PLUS",
    "NUM_EXP_MINUS",
    "NUM_EXP_DIGIT",
    "ERR_LEADING_ZEROES",
    "ERR_PLUS_SIGN",
    "ERR_NO_DIGIT_BEFORE_DECIMAL",
    "ERR_NO_DIGIT_BEFORE_EXPONENT",
    "ERR_NOT_A_NUMBER",
    "ERR_DOUBLE_DECIMAL",
    "ERR_DOUBLE_EXPONENT",
    "NUM_END",
    "JSON_BV_STATE_SIZE"
]

state_index = {state: i for i, state in enumerate(states)}

INPUT_RANGE = 256

ALL = [c for c in range(256)]
def all_except_for(l):
    return [c for c in range(256) if c not in l]

END_CHARS = [ord('}'), ord(']'), ord(','), ord(' '), ord('\t'), ord('\r'), ord('\n')]

def asc(c):
    return [ord(c)]

def one_of(l):
    return [ord(c) for c in l]

VALID_UTF8_1BYTE = [x for x in range(256) if (0x00 <= x <= 0x7F) or (0xC0 <= x <= 0xDF) or (0xE0 <= x <= 0xEF) or (0xF0 <= x <= 0xF7)]

UTF8_1BYTE = [c for c in range(31, 128) if c not in [ord('"'), ord('\\'), ord('\t'),
                                                 ord('\n'), ord('\r'), ord('\f'),
                                                 ord('\b')] ]
UTF8_2BYTE = [c for c in range(0xC0, 0xE0)]
UTF8_3BYTE = [c for c in range(0xE0, 0xF0)]
UTF8_4BYTE = [c for c in range(0xF0, 0xF8)]
CONT_BYTE = [c for c in range(0x80, 0xC0)]
VALID_ESCAPES = [ord(c) for c in 'bftnru\\/"']
HEX_DIGITS = [ord(c) for c in '0123456789aAbBcCdDeEfF']
HS_NIBBLE = [ord(c) for c in '89aAbB']
LS_NIBBLE = [ord(c) for c in 'cCdDeEfF']

def string_starting_bytes(state):
    return [(state, [(UTF8_1BYTE, "STR_UTF_1BYTE"),
                     (UTF8_2BYTE, "STR_UTF_2BYTE_1"),
                     (UTF8_3BYTE, "STR_UTF_3BYTE_1"),
                     (UTF8_4BYTE, "STR_UTF_4BYTE_1"),
                     (asc('\\'), "STR_CONTROL_SLASH"),
                     (asc('"'), "STR_QUOTE_END"),
                     (all_except_for(VALID_UTF8_1BYTE), "ERR_INVALID_UTF8")])]

transitions = [
    ("START", [(asc('n'), "NULL_N"),
               (asc('t'), "BOOL_T"),
               (asc('f'), "BOOL_F"),
               (asc('"'), "STR_QUOTE_BEGIN"),
               (asc('0'), "NUM_ZERO"),
               (one_of('123456789'), "NUM_DIGIT19"),
               (asc('-'), "NUM_MINUS"),
               (asc('.'), "ERR_NO_DIGIT_BEFORE_DECIMAL"),
               (one_of('eE'), "ERR_NO_DIGIT_BEFORE_EXPONENT"),
               (asc('+'), "ERR_PLUS_SIGN"),
               (all_except_for(one_of('ntf"0123456789-.eE+')), "ERR_INVALID_START")]),
    ("NULL_N", [(asc('u'), "NULL_U"),
                (all_except_for(asc('u')), "ERR_EXPECTED_NULL")]),
    ("NULL_U", [(asc('l'), "NULL_L1"),
                (all_except_for(asc('l')), "ERR_EXPECTED_NULL")]),
    ("NULL_L1", [(asc('l'), "NULL_L2"),
                 (all_except_for(asc('l')), "ERR_EXPECTED_NULL")]),
    ("NULL_L2", [(ALL, "NULL_L2")]),
    ("BOOL_T", [(asc('r'), "BOOL_R"),
                (all_except_for(asc('r')), "ERR_EXPECTED_TRUE")]),
    ("BOOL_R", [(asc('u'), "BOOL_U"),
                (all_except_for(asc('u')), "ERR_EXPECTED_TRUE")]),
    ("BOOL_U", [(asc('e'), "BOOL_TE"),
                (all_except_for(asc('e')), "ERR_EXPECTED_TRUE")]),
    ("BOOL_TE",[(ALL, "BOOL_TE")]),
    ("BOOL_F", [(asc('a'), "BOOL_A"),
                (all_except_for(asc('a')), "ERR_EXPECTED_FALSE")]),
    ("BOOL_A", [(asc('l'), "BOOL_L"),
                (all_except_for(asc('l')), "ERR_EXPECTED_FALSE")]),
    ("BOOL_L", [(asc('s'), "BOOL_S"),
                (all_except_for(asc('s')), "ERR_EXPECTED_FALSE")]),
    ("BOOL_S", [(asc('e'), "BOOL_FE"),
                (all_except_for(asc('e')), "ERR_EXPECTED_FALSE")]),
    ("BOOL_FE", [(ALL, "BOOL_FE")])
] + string_starting_bytes("STR_QUOTE_BEGIN") + string_starting_bytes("STR_UTF_1BYTE") + [
    ("STR_UTF_2BYTE_1", [(CONT_BYTE, "STR_UTF_2BYTE_2"),
                         (all_except_for(CONT_BYTE), "ERR_INVALID_UTF8")]),
] + string_starting_bytes("STR_UTF_2BYTE_2") + [
    ("STR_UTF_3BYTE_1", [(CONT_BYTE, "STR_UTF_3BYTE_2"),
                         (all_except_for(CONT_BYTE), "ERR_INVALID_UTF8")]),
    ("STR_UTF_3BYTE_2", [(CONT_BYTE, "STR_UTF_3BYTE_3"),
                         (all_except_for(CONT_BYTE), "ERR_INVALID_UTF8")]),
] + string_starting_bytes("STR_UTF_3BYTE_3") + [
    ("STR_UTF_4BYTE_1", [(CONT_BYTE, "STR_UTF_4BYTE_2"),
                         (all_except_for(CONT_BYTE), "ERR_INVALID_UTF8")]),
    ("STR_UTF_4BYTE_2", [(CONT_BYTE, "STR_UTF_4BYTE_3"),
                         (all_except_for(CONT_BYTE), "ERR_INVALID_UTF8")]),
    ("STR_UTF_4BYTE_3", [(CONT_BYTE, "STR_UTF_4BYTE_4"),
                         (all_except_for(CONT_BYTE), "ERR_INVALID_UTF8")]),
] + string_starting_bytes("STR_UTF_4BYTE_4") + [
    ("STR_CONTROL_SLASH", [(asc('\\'), "STR_CONTROL_REVSOL"),
                           (asc('/'), "STR_CONTROL_SOL"),
                           (asc('"'), "STR_CONTROL_QUOTE"),
                           (asc('b'), "STR_CONTROL_B"),
                           (asc('f'), "STR_CONTROL_F"),
                           (asc('n'), "STR_CONTROL_N"),
                           (asc('r'), "STR_CONTROL_R"),
                           (asc('t'), "STR_CONTROL_T"),
                           (asc('u'), "STR_CONTROL_U"),
                           (all_except_for(VALID_ESCAPES), "ERR_INVALID_ESCAPE_CHAR")]),
] + string_starting_bytes("STR_CONTROL_REVSOL") + string_starting_bytes("STR_CONTROL_SOL") + string_starting_bytes("STR_CONTROL_QUOTE") + string_starting_bytes("STR_CONTROL_B") + string_starting_bytes("STR_CONTROL_F") + string_starting_bytes("STR_CONTROL_N") + string_starting_bytes("STR_CONTROL_R") + string_starting_bytes("STR_CONTROL_T") + [
    ("STR_CONTROL_U", [([c for c in HEX_DIGITS if c != ord('d') and c != ord('D')], "STR_UNICODE_HEX1"),
                       (one_of('dD'), "STR_UNICODE_MAYBE_HS_HEX1"),
                       (all_except_for(HEX_DIGITS), "ERR_INVALID_UNICODE_ESCAPE")]),
    ("STR_UNICODE_HEX1", [(HEX_DIGITS, "STR_UNICODE_HEX2"),
                          (all_except_for(HEX_DIGITS), "ERR_INVALID_UNICODE_ESCAPE")]),
    ("STR_UNICODE_HEX2", [(HEX_DIGITS, "STR_UNICODE_HEX3"),
                          (all_except_for(HEX_DIGITS), "ERR_INVALID_UNICODE_ESCAPE")]),
    ("STR_UNICODE_HEX3", [(HEX_DIGITS, "STR_UNICODE_HEX4"),
                          (all_except_for(HEX_DIGITS), "ERR_INVALID_UNICODE_ESCAPE")]),
] + string_starting_bytes("STR_UNICODE_HEX4") + [
    ("STR_UNICODE_MAYBE_HS_HEX1", [(HS_NIBBLE, "STR_UNICODE_HS_HEX2"),
                                   (LS_NIBBLE, "ERR_LONE_SURROGATE"),
                                   ([c for c in HEX_DIGITS if c not in HS_NIBBLE + LS_NIBBLE], "STR_UNICODE_HEX2"),
                                   (all_except_for(HEX_DIGITS), "ERR_INVALID_UNICODE_ESCAPE")]),
    ("STR_UNICODE_HS_HEX2", [(HEX_DIGITS, "STR_UNICODE_HS_HEX3"),
                             (all_except_for(HEX_DIGITS), "ERR_INVALID_UNICODE_ESCAPE")]),
    ("STR_UNICODE_HS_HEX3", [(HEX_DIGITS, "STR_UNICODE_HS_HEX4"),
                             (all_except_for(HEX_DIGITS), "ERR_INVALID_UNICODE_ESCAPE")]),
    ("STR_UNICODE_HS_HEX4", [(asc('\\'), "STR_UNICODE_LS_CONTROL_SLASH"),
                             (all_except_for(asc('\\')), "ERR_UNPAIRED_SURROGATE")]),
    ("STR_UNICODE_LS_CONTROL_SLASH", [(asc('u'), "STR_UNICODE_LS_CONTROL_U"),
                                      (all_except_for(asc('u')), "ERR_UNPAIRED_SURROGATE")]),
    ("STR_UNICODE_LS_CONTROL_U", [(one_of('dD'), "STR_UNICODE_LS_HEX1"),
                                  (all_except_for(one_of('dD')), "ERR_UNPAIRED_SURROGATE")]),
    ("STR_UNICODE_LS_HEX1", [(LS_NIBBLE, "STR_UNICODE_LS_HEX2"),
                             (all_except_for(LS_NIBBLE), "ERR_UNPAIRED_SURROGATE")]),
    ("STR_UNICODE_LS_HEX2", [(HEX_DIGITS, "STR_UNICODE_LS_HEX3"),
                             (all_except_for(HEX_DIGITS), "ERR_UNPAIRED_SURROGATE")]),
    ("STR_UNICODE_LS_HEX3", [(HEX_DIGITS, "STR_UNICODE_LS_HEX4"),
                             (all_except_for(HEX_DIGITS), "ERR_UNPAIRED_SURROGATE")]),
] + string_starting_bytes("STR_UNICODE_LS_HEX4") + [
    ("NUM_MINUS", [(asc('0'), "NUM_ZERO"),
                   (one_of('123456789'), "NUM_DIGIT19"),
                   (all_except_for(one_of('0123456789')), "ERR_NOT_A_NUMBER")]),
    ("NUM_ZERO", [(asc('.'), "NUM_DECIMAL"),
                  (one_of('eE'), "NUM_EXPONENT"),
                  (one_of(' \t\r\n}],'), 'NUM_END'),
                  (one_of('0123456789'), "ERR_LEADING_ZEROES"),
                  (all_except_for(one_of('.eE0123456789 \t\r\n}],')), "ERR_NOT_A_NUMBER")]),
    ("NUM_DIGIT19", [(one_of('0123456789'), "NUM_DIGIT"),
                     (asc('.'), "NUM_DECIMAL"),
                     (one_of('eE'), "NUM_EXPONENT"),
                     (one_of(' \t\r\n}],'), 'NUM_END'),
                     (all_except_for(one_of('.eE0123456789,}]\t\r\n ')), "ERR_NOT_A_NUMBER")]),
    ("NUM_DIGIT", [(one_of('0123456789'), "NUM_DIGIT"),
                   (asc('.'), "NUM_DECIMAL"),
                   (one_of('eE'), "NUM_EXPONENT"),
                   (one_of(' \t\r\n}],'), 'NUM_END'),
                   (all_except_for(one_of('.eE0123456789},] \t\n\r')), "ERR_NOT_A_NUMBER")]),
    ("NUM_DECIMAL", [(one_of('0123456789'),"NUM_DECIMAL_DIGIT"),
                     (asc('.'), "ERR_DOUBLE_DECIMAL"),
                     (all_except_for(one_of('.0123456789')), "ERR_NOT_A_NUMBER")]),
    ("NUM_DECIMAL_DIGIT", [(one_of('0123456789'), 'NUM_DECIMAL_DIGIT'),
                           (one_of('eE'), 'NUM_EXPONENT'),
                           (one_of(' \t\r\n}],'), 'NUM_END'),
                           (asc('.'), 'ERR_DOUBLE_DECIMAL'),
                           (all_except_for(one_of('0123456789eE \t\r\n}],.')), 'ERR_NOT_A_NUMBER')]),
    ('NUM_EXPONENT', [(asc('+'), 'NUM_EXP_PLUS'),
                      (asc('-'), 'NUM_EXP_MINUS'),
                      (one_of('0123456789'), 'NUM_EXP_DIGIT'),
                      (one_of('eE'), 'ERR_DOUBLE_EXPONENT'),
                      (all_except_for(one_of('+-eE0123456789')), 'ERR_NOT_A_NUMBER')]),
    ('NUM_EXP_PLUS', [(one_of('0123456789'), 'NUM_EXP_DIGIT'),
                      (one_of('eE'), 'ERR_DOUBLE_EXPONENT'),
                      (all_except_for(one_of('0123456789eE')), 'ERR_NOT_A_NUMBER')]),
    ('NUM_EXP_MINUS', [(one_of('0123456789'), 'NUM_EXP_DIGIT'),
                       (one_of('eE'), 'ERR_DOUBLE_EXPONENT'),
                       (all_except_for(one_of('0123456789eE')), 'ERR_NOT_A_NUMBER')]),
    ('NUM_EXP_DIGIT', [(one_of('0123456789'), 'NUM_EXP_DIGIT'),
                       (one_of(' \t\n\r,}]'), 'NUM_END'),
                       (one_of('eE'), 'ERR_DOUBLE_EXPONENT'),
                       (all_except_for(one_of('0123456789eE \t\n\r,}]')), 'ERR_NOT_A_NUMBER')]),
    ('NUM_END', [(ALL, 'NUM_END')]),
    ('JSON_BV_STATE_SIZE', [(ALL, 'JSON_BV_STATE_SIZE')]),
    ('ERR_DOUBLE_EXPONENT', [(ALL, 'ERR_DOUBLE_EXPONENT')]),
    ('ERR_DOUBLE_DECIMAL', [(ALL, 'ERR_DOUBLE_DECIMAL')]),
    ('ERR_NOT_A_NUMBER', [(ALL, 'ERR_NOT_A_NUMBER')]),
    ('ERR_NO_DIGIT_BEFORE_EXPONENT', [(ALL, 'ERR_NO_DIGIT_BEFORE_EXPONENT')]),
    ('ERR_NO_DIGIT_BEFORE_DECIMAL', [(ALL, 'ERR_NO_DIGIT_BEFORE_DECIMAL')]),
    ('ERR_PLUS_SIGN', [(ALL, 'ERR_PLUS_SIGN')]),
    ('ERR_LEADING_ZEROES', [(ALL, 'ERR_LEADING_ZEROES')]),
    ('ERR_UNPAIRED_SURROGATE', [(ALL, 'ERR_UNPAIRED_SURROGATE')]),
    ('ERR_LONE_SURROGATE', [(ALL, 'ERR_LONE_SURROGATE')]),
    ('ERR_INVALID_UNICODE_ESCAPE', [(ALL, 'ERR_INVALID_UNICODE_ESCAPE')]),
    ('ERR_INVALID_ESCAPE_CHAR', [(ALL, 'ERR_INVALID_ESCAPE_CHAR')]),
    ('ERR_INVALID_UTF8', [(ALL, 'ERR_INVALID_UTF8')]),
    ('ERR_EXPECTED_NULL', [(ALL, 'ERR_EXPECTED_NULL')]),
    ('ERR_EXPECTED_TRUE', [(ALL, 'ERR_EXPECTED_TRUE')]),
    ('ERR_EXPECTED_FALSE', [(ALL, 'ERR_EXPECTED_FALSE')]),
    ('ERR_INVALID_START', [(ALL, 'ERR_INVALID_START')]),
]

## print out the transitions as an initializer list

init_list = ''
for state_from, tran_to in transitions:
    line = '[' + state_from + '] = { \n'
    for t_list, state_to in tran_to:
        line = line + ',\n'.join([f"[{t_list[i]}] = {state_to}" for i in range(len(t_list))]) + ',\n'
    line = line + '},\n'
    init_list = init_list + line

print(init_list)
