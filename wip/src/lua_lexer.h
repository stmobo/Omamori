// lua_lexer.h -- interface to the lua lexer

// the below are just for convenience / debugging - the actual version as used in the kernel
// can't and won't use any standard libraries.
#include <iostream>
#include <cstring>
#include <string>

enum struct lex_token : char {
    error_unrecognized_character,
    error_unmatched_quote,
    cmp_less_or_equal,
    cmp_greater_or_equal,
    cmp_equal,
    cmp_unequal,
    cmp_less,
    cmp_greater,
    oper_assignment,
    oper_plus,
    oper_minus,
    oper_multiply,
    oper_divide,
    oper_exp,
    oper_modulus,
    oper_inverse,
    oper_len,
    oper_not,
    oper_or,
    oper_and,
    value_true,
    value_false,
    value_nil,
    keyword_do,
    keyword_while,
    keyword_repeat,
    keyword_until,
    keyword_if,
    keyword_then,
    keyword_elseif,
    keyword_else,
    keyword_for,
    keyword_local,
    keyword_function,
    keyword_return,
    keyword_break,
    keyword_end,
    keyword_in,
    open_squarebracket,
    close_squarebracket,
    open_paren,
    close_paren,
    open_curlybrace,
    close_curlybrace,
    whitespace,
    identifier,
    numeric_literal,
    string,
    comma,
    semicolon,
    period,
    colon,
    ellipsis,
    nothing,
};

extern const char* token_to_str( lex_token );
extern lex_token lex_next(const char*, int*, int, char**);
extern void lex(std::string str);