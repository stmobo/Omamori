// lua_parser.cpp
// handwritten recursive descent parser for Lua

#include "lua_lexer.h"

lex_token t = lex_token::whitespace;
lex_token t_lookahead = lex_token::whitespace;
char *t_str;
char *lookahead_str;

bool chunk(const char*, int*, int);
bool block(const char*, int*, int);
bool stat(const char*, int*, int);
bool laststat(const char*, int*, int);
bool funcname(const char*, int*, int);
bool varlist(const char*, int*, int);
bool var(const char*, int*, int);
bool namelist(const char*, int*, int);
bool explist(const char*, int*, int);
bool exp(const char*, int*, int);
bool prefixexp(const char*, int*, int);
bool functioncall(const char*, int*, int);
bool args(const char*, int*, int);
bool function(const char*, int*, int);
bool funcbody(const char*, int*, int);
bool parlist(const char*, int*, int);
bool tableconstructor(const char*, int*, int);
bool fieldlist(const char*, int*, int);
bool field(const char*, int*, int);
bool fieldsep(const char*, int*, int);
bool binop(const char*, int*, int);
bool unop(const char*, int*, int);

void getsym(const char* str, int *pos, int len) {
    t_str = lookahead_str;
    t = t_lookahead;
    
    t_lookahead = lex_next(str, pos, len, &lookahead_str);
}

bool chunk(const char* str, int *pos, int len) {
    while( stat(str, pos, len) ) {
        if( t == lex_token::semicolon ) {
            getsym(str, pos, len);
        } else {
            return false;
        }
    }
    if( laststat(str, pos, len) ) {
        if( t == lex_token::semicolon ) {
            getsym(str, pos, len);
        }
    }
    return true;
}

bool block(const char* str, int *pos, int len) {
    return chunk(str, pos, len);
}

bool stat(const char* str, int *pos, int len) {
    switch(t) {
        case lex_token::keyword_do:
            getsym(str, pos, len);
            if( block(str, pos, len) ) {
                if( t == lex_token::keyword_end ) {
                    getsym(str, pos, len);
                    return true;
                }
            }
            return false;
        case lex_token::keyword_while:
            getsym(str, pos, len);
            if( exp(str, pos, len) ) {
                if( t == lex_token::keyword_do ) {
                    getsym(str, pos, len);
                    if( block(str, pos, len) ) {
                        if( t == lex_token::keyword_end ) {
                            getsym(str, pos, len);
                            return true;
                        }
                    }
                }
            }
            return false;
        case lex_token::keyword_repeat:
            getsym(str, pos, len);
            if( block(str, pos, len) ) {
                if( t == lex_token::keyword_until ) {
                    getsym(str, pos, len);
                    return exp(str, pos, len);
                }
            }
            return false;
        case lex_token::keyword_if:
            getsym(str, pos, len);
            if( exp(str, pos, len) ) {
                if( t == lex_token::keyword_then ) {
                    getsym(str, pos, len);
                    if( block(str, pos, len) ) {
                        while( t == lex_token::keyword_elseif ) {
                            getsym(str, pos, len);
                            if( exp(str, pos, len) ) {
                                if( t == lex_token::keyword_then ) {
                                    getsym(str, pos, len);
                                    if( block(str, pos, len) ) {
                                        ;// do whatever
                                    } else {
                                        return false;
                                    }
                                } else {
                                    return false;
                                }
                            } else {
                                return false;
                            }
                        }
                        if( t == lex_token::keyword_else ) {
                            getsym(str, pos, len);
                            if( block(str, pos, len) ) {
                                // do whatever
                            } else {
                                return false;
                            }
                        }
                        if( t == lex_token::keyword_end ) {
                            getsym(str, pos, len);
                            return true;
                        }
                    }
                }
            }
            return false;
        case lex_token::keyword_for:
            getsym(str, pos, len);
            if( t == lex_token::identifier ) {
                getsym(str, pos, len);
                if( t == lex_token::oper_assignment ) {
                    getsym(str, pos, len);
                    if( exp(str, pos, len) ) {
                        if( t == lex_token::comma ) {
                            getsym(str, pos, len);
                            if( exp(str, pos, len) ) {
                                if( t == lex_token::comma ) {
                                    getsym(str, pos, len);
                                    if( exp(str, pos, len) ){
                                        ; // do whatever
                                    } else {
                                        return false;
                                    }
                                }
                                if( t == lex_token::keyword_do ) {
                                    getsym(str, pos, len);
                                    if( block(str, pos, len) ) {
                                        if( t == lex_token::keyword_end ) {
                                            getsym(str, pos, len);
                                            return true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            } else if( namelist(str, pos, len) ) {
                if( t == lex_token::keyword_in ) {
                    getsym(str, pos, len);
                    if( explist(str, pos, len) ) {
                        if( t == lex_token::keyword_do ) {
                            getsym(str, pos, len);
                            if( block(str, pos, len) ) {
                                if( t == lex_token::keyword_end ) {
                                    getsym(str, pos, len);
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
            return false;
        case lex_token::keyword_function:
            getsym(str, pos, len);
            if( funcname(str, pos, len) ) {
                return funcbody(str, pos, len);
            }
            return false;
        case lex_token::keyword_local:
            getsym(str, pos, len);
            if( t == lex_token::keyword_function ) {
                getsym(str, pos, len);
                if( t == lex_token::identifier ) {
                    getsym(str, pos, len);
                    return funcbody(str, pos, len);
                }
            } else if ( namelist(str, pos, len) ) {
                if( t == lex_token::oper_assignment ) {
                    getsym(str, pos, len);
                    return explist(str, pos, len);
                }
                return true;
            }    
            return false;
        default:
            if( functioncall(str, pos, len) ) {
                return true;
            } else if( varlist(str, pos, len) ) {
                if( t == lex_token::oper_assignment ) {
                    getsym(str, pos, len);
                    return explist(str, pos, len);
                }
            }
            return false;
    }
}

bool laststat(const char* str, int *pos, int len) {
    if( t == lex_token::keyword_return ) {
        getsym(str, pos, len);
        if( explist(str, pos, len) ) {
            ; // do whatever
        }
        return true;
    } else if( t == lex_token::keyword_break ) {
        getsym(str, pos, len);
        return true;
    }
    return false;
}

bool funcname(const char* str, int *pos, int len) {
    if( t == lex_token::identifier ) {
        getsym(str, pos, len);
        while( t == lex_token::period ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier ) {
                getsym(str, pos, len);
            } else {
                return false;
            }
        }
        if( t == lex_token::colon ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier ) {
                // okay, do stuff here
                getsym(str, pos, len);
            } else {
                return false;
            }
        }
        return true;
    }
}

bool varlist(const char* str, int *pos, int len) {
    if( var(str, pos, len) ) {
        while( t == lex_token::comma ) {
            var(str, pos, len);
            getsym(str, pos, len);
        }
        return true;
    }
    return false;
}

bool var(const char* str, int *pos, int len) {
    if( t == lex_token::identifier ) {
        getsym(str, pos, len);
        return true;
    } else if( prefixexp(str, pos, len) ) {
        if( t == lex_token::open_squarebracket ) {
            if( exp(str, pos, len) ) {
                if( t == lex_token::close_squarebracket ) {
                    getsym(str, pos, len);
                    return true;
                }
            }
        } else if( t == lex_token::period ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier ) {
                getsym(str, pos, len);
                return true;
            }
        }
    }
    return false;
}

bool namelist(const char* str, int *pos, int len) {
    if( exp(str, pos, len) ) {
        while( t == lex_token::comma ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier) {
                // t is now an identifier.
                getsym(str, pos, len);
            } else {
                return false;
            }
        }
        return true;
    }
    return false;
}

bool explist(const char* str, int *pos, int len) {
    if( exp(str, pos, len) ) {
        while( t == lex_token::comma ) {
            exp(str, pos, len);
            getsym(str, pos, len);
        }
        return true;
    }
    return false;
}

bool exp(const char* str, int *pos, int len) {
    if( t == lex_token::value_nil || t == lex_token::value_false || t == lex_token::value_true || t == lex_token::numeric_literal || t == lex_token::string || t == lex_token::ellipsis ) {
        getsym(str, pos, len);
        return true;
    } else if( function(str, pos, len) ) {
        return true;
    } else if( prefixexp(str, pos, len) ) {
        return true;
    } else if( tableconstructor(str, pos, len) ) {
        return true;
    } else if( exp(str, pos, len) ) {
        if( binop(str, pos, len) ) {
            if( exp(str, pos, len) ) {
                return true;
            }
        }
    } else if( unop(str, pos, len) ) {
        return true;
    }
    return false;
}

bool prefixexp(const char* str, int *pos, int len) {
    if( var(str, pos, len) ) {
        return true;
    } else if( functioncall(str, pos, len) ) {
        return true;
    } else if ( t == lex_token::open_paren ) {
        getsym(str, pos, len);
        if( exp(str, pos, len) ) {
            if( t == lex_token::close_paren ) {
                getsym(str, pos, len);
                return true;
            }
        }
    }
    return false;
}

bool functioncall(const char* str, int *pos, int len) {
    if( prefixexp(str, pos, len) ) {
        if( t == lex_token::colon ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier ) {
                getsym(str, pos, len);
                ; // do stuff here
            }
        }
        if( args(str, pos, len) ) {
            return true;
        }
    }
    return false;
}

bool args(const char* str, int *pos, int len) {
    if( t == lex_token::open_paren ) {
        getsym(str, pos, len);
        if( explist(str, pos, len) ) {
            ; // do stuff here
        }
        if( t == lex_token::close_paren ) {
            getsym(str, pos, len);
            return true;
        }
    } else if( tableconstructor(str, pos, len) ) {
        getsym(str, pos, len);
        return true;
    } else if( t == lex_token::string ) {
        getsym(str, pos, len);
        return true;
    }
    return false;
}

bool function(const char* str, int *pos, int len) {
    if( t == lex_token::keyword_function ) {
        getsym(str, pos, len);
        return funcbody(str, pos, len);
    }
    return false;
}

bool funcbody(const char* str, int *pos, int len) {
    if( t == lex_token::open_paren ) {
        getsym(str, pos, len);
        if( parlist(str, pos, len) ) {
            ; /* stuff here */
        }
        if( t == lex_token::close_paren ) {
            getsym(str, pos, len);
            if( block(str, pos, len) ) {
                if( t == lex_token::keyword_end ) {
                    getsym(str, pos, len);
                    return true;
                }
            }
        }
    }
    return false;
}

bool parlist(const char* str, int *pos, int len) {
    if( t == lex_token::ellipsis ) {
        getsym(str, pos, len);
        return true;
    } else if( namelist(str, pos, len) ) {
        if( t == lex_token::comma ) {
            getsym(str, pos, len);
            if( t == lex_token::ellipsis ) {
                getsym(str, pos, len);
                return true;
            }
        }
    }
    return false;
}

bool tableconstructor(const char* str, int *pos, int len) {
    if( t == lex_token::open_curlybrace ) { 
        getsym(str, pos, len);
        fieldlist(str, pos, len); // optional element
        if( t == lex_token::close_curlybrace ) {
            getsym(str, pos, len);
            return true;
        }
    }
    return false;
}

bool fieldlist(const char* str, int *pos, int len) {
    if( field(str, pos, len) ) {
        getsym(str, pos, len);
        while( fieldsep(str, pos, len) ) { // is the current character a field separator?
            field(str, pos, len); // let's hope the next character(s) are a field.
            getsym(str, pos, len);
        }
        if( fieldsep(str, pos, len) ) {
            ; // stuff here
        }
        return true;
    }
    return false;
}

bool field(const char* str, int *pos, int len) {
    if( t == lex_token::open_squarebracket ){
        getsym(str, pos, len);
        if(exp()) {
            if( t == lex_token::close_squarebracket && t_lookahead == lex_token::oper_assignment ) {
                getsym(str, pos, len);
                return exp();
            }
            return false;
        }
    } else {    
        if(exp()) {
            if(t == lex_token::oper_assignment) {
                getsym(str, pos, len);
                return exp();
            }
            getsym(str, pos, len);
            return true;
        }
    }
    return false;
}

bool fieldsep(const char* str, int *pos, int len) {
    if(t == lex_token::comma) {
        getsym(str, pos, len);
        return true;
    } else if(t == lex_token::semicolon) {
        getsym(str, pos, len);
        return true;
    }
    return false;
}

bool binop(const char* str, int *pos, int len) {
    if(t == lex_token::oper_plus || t == lex_token::oper_minus || t == lex_token::oper_multiply || t == lex_token::oper_divide || t == lex_token::oper_exp || t == lex_token::oper_modulus || t == lex_token::cmp_less || t == lex_token::cmp_greater || t == lex_token::cmp_less_or_equal || t == lex_token::cmp_greater_or_equal || t == lex_token::mp_equal || t == lex_token::cmp_unequal || t == lex_token::oper_and || t == lex_token::oper_or) {
        // do stuff here -- we've found a binary operator.
        getsym(str, pos, len);
        return true;
    }
}

bool unop(const char* str, int *pos, int len) {
    if(t == lex_token::oper_inverse || t == lex_token::oper_not || t == lex_token::oper_len) {
        // do stuff here -- we've found an unary operator.
        getsym(str, pos, len);
        return true;
    }
    return false;
}

int main() {
    return 0;
}