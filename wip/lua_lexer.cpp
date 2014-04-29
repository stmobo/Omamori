// This isn't meant to go into the main repo. Not yet.

#include "lua_lexer.h"

// Lexer rules:
// An uninterrupted string of alphanumeric characters is a token.
// "<" and ">", and "=" are always tokens. However, if a "=" character comes immediately after it, it forms another token.
// "~" is only a token if a "=" comes after it.
// Parentheses are always tokens.
// "+-/*^%" are also always tokens.
// Periods are always tokens.
// Commas are always their own tokens.
// Semicolons are equivalent to whitespace.
// Brackets (of the curly and square variants) are also always tokens.
// Quote marks (and double square brackets) make 1 token out of everything between them.

const char* token_to_str( lex_token token ) {
    switch(token) {
        case lex_token::cmp_less_or_equal:
            return "less than or equal to";
        case lex_token::cmp_greater_or_equal:
            return "greater than or equal to";
        case lex_token::cmp_equal:
            return "equal to";
        case lex_token::cmp_unequal:
            return "not equal to";
        case lex_token::cmp_less:
            return "less than";
        case lex_token::cmp_greater:
            return "greater than";
        case lex_token::oper_assignment:
            return "assignment operator";
        case lex_token::oper_plus:
            return "addition operator";
        case lex_token::oper_minus:
            return "subtraction operator";
        case lex_token::oper_multiply:
            return "multiplication operator";
        case lex_token::oper_divide:
            return "division operator";
        case lex_token::oper_exp:
            return "exponentiation operator";
        case lex_token::oper_modulus:
            return "modulus operator";
        case lex_token::oper_inverse:
            return "inversion operator";
        case lex_token::oper_len:
            return "length operator";
        case lex_token::oper_not:
            return "NOT logical operator";
        case lex_token::oper_or:
            return "OR logical operator";
        case lex_token::oper_and:
            return "AND logical operator";
        case lex_token::value_true:
            return "constant true";
        case lex_token::value_false:
            return "constant false";
        case lex_token::value_nil:
            return "constant nil";
        case lex_token::keyword_do:
            return "do statement";
        case lex_token::keyword_while:
            return "while statement";
        case lex_token::keyword_repeat:
            return "repeat statement";
        case lex_token::keyword_until:
            return "until statement";
        case lex_token::keyword_if:
            return "if statement";
        case lex_token::keyword_then:
            return "then block";
        case lex_token::keyword_elseif:
            return "elseif block";
        case lex_token::keyword_else:
            return "else block";
        case lex_token::keyword_for:
            return "for statement";
        case lex_token::keyword_in:
            return "in statement";
        case lex_token::keyword_return:
            return "return statement";
        case lex_token::keyword_break:
            return "break statement";
        case lex_token::keyword_local:
            return "local";
        case lex_token::keyword_function:
            return "function definition";
        case lex_token::keyword_end:
            return "keyword end";
        case lex_token::comma:
            return "comma";
        case lex_token::colon:
            return "colon";
        case lex_token::semicolon:
            return "semicolon";
        case lex_token::open_squarebracket:
            return "open square bracket";
        case lex_token::close_squarebracket:
            return "close square bracket";
        case lex_token::period:
            return "period";
        case lex_token::string:
            return "string literal";
        case lex_token::open_paren:
            return "open parenthesis";
        case lex_token::close_paren:
            return "close parenthesis";
        case lex_token::open_curlybrace:
            return "open curly brace";
        case lex_token::close_curlybrace:
            return "close curly brace";
        case lex_token::identifier:
            return "identifier";
        case lex_token::numeric_literal:
            return "numeric literal";
        case lex_token::error_unmatched_quote:
            return "Error: unmatched quote";
        case lex_token::error_unrecognized_character:
            return "Error: unrecognized character";
        case lex_token::nothing:
            return "nothing";
        case lex_token::whitespace:
            return "whitespace";
        default:
            return "Error: unknown token type";
    }
}

lex_token resolve_alphanumeric_sequence(char* str) {
    const char* lhs = const_cast<char*>(str);
    if(std::strcmp( lhs, "and" ) == 0)
        return lex_token::oper_and;
    if(std::strcmp( lhs, "or" ) == 0)
        return lex_token::oper_or;
    if(std::strcmp( lhs, "not" ) == 0)
        return lex_token::oper_not;
    if(std::strcmp( lhs, "do" ) == 0)
        return lex_token::keyword_do;
    if(std::strcmp( lhs, "while" ) == 0)
        return lex_token::keyword_while;
    if(std::strcmp( lhs, "repeat" ) == 0)
        return lex_token::keyword_repeat;
    if(std::strcmp( lhs, "until" ) == 0)
        return lex_token::keyword_until;
    if(std::strcmp( lhs, "if") == 0)
        return lex_token::keyword_if;
    if(std::strcmp( lhs, "then" ) == 0)
        return lex_token::keyword_then;
    if(std::strcmp( lhs, "elseif" ) == 0)
        return lex_token::keyword_elseif;
    if(std::strcmp( lhs, "else" ) == 0)
        return lex_token::keyword_else;
    if(std::strcmp( lhs, "for" ) == 0)
        return lex_token::keyword_for;
    if(std::strcmp( lhs, "function" ) == 0)
        return lex_token::keyword_function;
    if(std::strcmp( lhs, "local" ) == 0)
        return lex_token::keyword_local;
    if(std::strcmp( lhs, "end" ) == 0)
        return lex_token::keyword_end;
    if(std::strcmp( lhs, "return" ) == 0)
        return lex_token::keyword_return;
    if(std::strcmp( lhs, "break" ) == 0)
        return lex_token::keyword_break;
    if(std::strcmp( lhs, "in" ) == 0)
        return lex_token::keyword_in;
    if( lhs[0] == '#' )
        return lex_token::oper_len;
    return lex_token::identifier;
}

lex_token lex_next(const char* str, int *pos, int len, char **state_data) {
    if(str[*pos] == '\0') {
        *pos += 1;
        return lex_token::whitespace;
    }
    //std::cout << "*pos=" << *pos << std::endl;
    //std::cout << "len=" << len << std::endl;
    //std::cout << "str[*pos]=" << str[*pos] << std::endl;
    if( (((str[*pos] >= 0x41) && (str[*pos] <= 0x5A)) || ((str[*pos] >= 0x61) && (str[*pos] <= 0x7A))) ) {
        int str_len = 0;
        //std::cout << "Reading identifier." << std::endl;
        for(int i=*pos;i<len;i++) {
            //std::cout << "i=" << i << std::endl;
            if( !(((str[i] >= 0x41) && (str[i] <= 0x5A)) || ((str[i] >= 0x61) && (str[i] <= 0x7A)) || ((str[i] >= 0x30) && (str[i] <= 0x39)) || (str[i] == '_')) ) {
                //std::cout << "Found end of identifier." << std::endl;
                str_len = i;
                break;
            }
        }
        //std::cout << "(str_len-*pos)=" << (str_len-*pos) << std::endl;
        char *data = new char[(str_len-*pos)+1];
        data[(str_len-*pos)] = '\0';
        for(int i=*pos;i<str_len;i++) {
            //std::cout << "(i-*pos)=" << (i-*pos) << std::endl;
            data[i-*pos] = str[i];
        }
        *state_data = data;
        *pos = str_len;
        return resolve_alphanumeric_sequence(data);
    } else if( ((str[*pos] >= 0x30) && (str[*pos] <= 0x39)) ) {
        if( str[*pos] =='0' ) {
            if( !(*pos+1 >= len) ) {
                if(str[*pos+1] == 'x') {
                    int str_len = 0;
                    //std::cout << "Reading hexadecimal numeric literal." << std::endl;
                    for(int i=*pos;i<len;i++) {
                        //std::cout << "i=" << i << std::endl;
                        if( !(((str[i] >= 0x30) && (str[i] <= 0x39)) || ((str[i] >= 0x41) && (str[i] <= 0x46)) || ((str[i] >= 0x61) && (str[i] <= 0x66))) ) {
                            //std::cout << "Found end of identifier." << std::endl;
                            str_len = i-1;
                            break;
                        }
                    }
                    *state_data = new char[(str_len-*pos)+1];
                    *state_data[(str_len-*pos)] = '\0';
                    for(int i=*pos;i<str_len;i++) {
                        *state_data[i-*pos] = str[i];
                    }
                    *pos = str_len;
                }
            }
        } else {
            //std::cout << "Reading decimal numeric literal." << std::endl;
            int str_len = 0;
            for(int i=*pos;i<len;i++) {
                //std::cout << "i=" << i << std::endl;
                if( !((str[i] >= 0x30) && (str[i] <= 0x39)) ) {
                    //std::cout << "Found end of literal." << std::endl;
                    str_len = i;
                    break;
                }
            }
            //std::cout << "Allocating *state_data." << std::endl;
            char *data = new char[(str_len-*pos)+1];
            //std::cout << "Adding null terminator to index " << (str_len-*pos) << std::endl;
            data[(str_len-*pos)] = '\0';
            //std::cout << "Copying to *state_data." << std::endl;
            for(int i=*pos;i<str_len;i++) {
                //std::cout << "i=" << i << std::endl;
                data[i-*pos] = str[i];
            }
            *state_data = data;
            *pos = str_len;
        }
        return lex_token::numeric_literal;
    } else {
        switch(str[*pos]) {
            case '<':
                if(str[*pos+1] == '=') {
                    *pos += 2;
                    return lex_token::cmp_less_or_equal;
                } else {
                    *pos += 1;
                    return lex_token::cmp_less;
                }
            case '>':
                if(str[*pos+1] == '=') {
                    *pos += 2;
                    return lex_token::cmp_greater_or_equal;
                } else {
                    *pos += 1;
                    return lex_token::cmp_greater;
                }
            case '=':
                if(str[*pos+1] == '=') {
                    *pos += 2;
                    return lex_token::cmp_equal;
                } else {
                    *pos += 1;
                    return lex_token::oper_assignment;
                }
                break;
            case '~':
                if(str[*pos+1] == '=') {
                    *pos += 2;
                    return lex_token::cmp_unequal;
                } else {
                    *pos += 1;
                    return lex_token::error_unrecognized_character;
                }
                break;
            case '.':
                *pos += 1;
                return lex_token::period;
            case '(':
                *pos += 1;
                return lex_token::open_paren;
            case ')':
                *pos += 1;
                return lex_token::close_paren;
            case '{':
                *pos += 1;
                return lex_token::open_curlybrace;
            case '}':
                *pos += 1;
                return lex_token::close_curlybrace;
            case '+':
                *pos += 1;
                return lex_token::oper_plus;
            case '-':
                if( ((str[*pos+1] >= 0x30) && (str[*pos+1] <= 0x39)) && ((*pos == 0) || !((str[*pos-1] >= 0x30) && (str[*pos-1] <= 0x39))) ) { // if the very next character is a numeric digit and the previous digit wasn't...
                    *pos += 1;
                    return lex_token::oper_inverse;
                }
                *pos += 1;
                return lex_token::oper_minus;
            case '*':
                *pos += 1;
                return lex_token::oper_multiply;
            case '/':
                *pos += 1;
                return lex_token::oper_divide;
            case '^':
                *pos += 1;
                return lex_token::oper_exp;
            case '%':
                *pos += 1;
                return lex_token::oper_modulus;
            case '#':
                *pos += 1;
                return lex_token::oper_len;
            case ',':
                *pos += 1;
                return lex_token::comma;
            case ':':
                *pos += 1;
                return lex_token::colon;
            case ';':
                *pos += 1;
                return lex_token::semicolon;
            case ' ':
                *pos += 1;
                return lex_token::whitespace;
            case ']':
                *pos += 1;
                return lex_token::close_squarebracket;
            case '[':
                if(str[*pos+1] != '[') {
                    *pos += 2;
                    return lex_token::open_squarebracket;
                } else {
                    // do our own pattern matching here
                    int str_len = 0;
                    *state_data = NULL;
                    for(int i=*pos;i<len;i++) {
                        if(str[i] == ']' && str[i+1] == ']' && str[i-1] != '\\') {
                            str_len = i;
                            break;
                        }
                    }
                    char *data = new char[(str_len-*pos)];
                    data[(str_len-*pos)] = '\0';
                    if(data == NULL) {
                        return lex_token::error_unmatched_quote;
                    }
                    for(int i=*pos;i<str_len+2;i++) {
                        data[i-*pos] = str[i];
                    }
                    *state_data = data;
                    *pos = str_len+2;
                    return lex_token::string;
                    }
            case '\"':
            case '\'':
                { // Have to have this code in its own block -- g++ throws errors if we don't
                    char starter_char = str[*pos];
                    int str_len = 0;
                    *state_data = NULL;
                    for(int i=*pos+1;i<len;i++) {
                        if(str[i] == starter_char && str[i-1] != '\\') {
                            str_len = i;
                            break;
                        }
                    }
                    char *data = new char[(str_len-*pos)+1];
                    data[(str_len-*pos)] = '\0';
                    if(data == NULL) {
                        return lex_token::error_unmatched_quote;
                    }
                    for(int i=*pos;i<str_len+1;i++) {
                        data[i-*pos] = str[i];
                    }
                    *state_data = data;
                    *pos = str_len+1;
                    return lex_token::string;
                }
            default:
                *pos += 1;
                return lex_token::error_unrecognized_character;
        }
        *pos += 1;
        return lex_token::whitespace;
    }
    return lex_token::nothing; // we should not get here
}

void lex(std::string str) {
    //std::cout << "Getting line length." << std::endl;
    int len = str.length()+1; // length of printable characters + null terminator
    int pos = 0;
    char *returned_string = NULL;
    while(pos < len) {
        lex_token t = lex_next(str.c_str(), &pos, len, &returned_string);
        if(t != lex_token::whitespace) {
            if(t == lex_token::identifier || t == lex_token::numeric_literal || t == lex_token::string) {
                std::cout << token_to_str(t) << " (" << std::string(returned_string) << ")" << std::endl;
                delete[] returned_string;
                *returned_string = NULL;
            } else {
                std::cout << token_to_str(t) << std::endl;
            }
        }
    }
}

/*
int main() {
    lex(std::string("if (t >= -5) and (#z ~= 51515) then a = b.some_function(\"this is a string\", [[this is a string \"with quotes\" in it.]]) elseif t < 5 then f2 = f.another_function(52, 33) end"));
}
*/