// lua_parser.cpp
// handwritten recursive descent parser for Lua

#include "lua_lexer.h"
#include <cassert>

lex_token t = lex_token::nothing;
char *t_str = NULL;

int recursion_depth = 0;

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
bool primaryexp(const char*, int*, int);
bool rest(const char*, int*, int);
bool functioncall(const char*, int*, int);
bool assignment_or_call(const char*, int*, int);
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

/*

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

*/

void getsym(const char* str, int *pos, int len) {
    if(t == lex_token::identifier || t == lex_token::numeric_literal || t == lex_token::string) {
        delete[] t_str;
    }
    
    lex_token t_candidate = lex_next(str, pos, len, &t_str);
    while( t_candidate == lex_token::whitespace ) {
        t_candidate = lex_next(str, pos, len, &t_str);
        std::cout << "T-candidate @ " << *pos <<  ": " << token_to_str( t_candidate ) << std::endl;
    }
    t = t_candidate;
}

bool chunk(const char* str, int *pos, int len) {
    std::cout << "Entering CHUNK." << std::endl;
    while( stat(str, pos, len) ) {
        if( t == lex_token::semicolon ) {
            getsym(str, pos, len);
        } else {
            std::cout << "Exiting CHUNK." << std::endl;
            return false;
        }
    }
    if( laststat(str, pos, len) ) {
        if( t == lex_token::semicolon ) {
            getsym(str, pos, len);
        }
    }
    std::cout << "Exiting CHUNK." << std::endl;
    return true;
}

bool block(const char* str, int *pos, int len) {
    return chunk(str, pos, len);
}

bool stat(const char* str, int *pos, int len) {
    std::cout << "Entering STATEMENT." << std::endl;
    switch(t) {
        case lex_token::keyword_do:
            getsym(str, pos, len);
            if( block(str, pos, len) ) {
                if( t == lex_token::keyword_end ) {
                    getsym(str, pos, len);
                    std::cout << "Exiting STATEMENT." << std::endl;
                    return true;
                }
            }
            std::cout << "Exiting STATEMENT." << std::endl;
            return false;
        case lex_token::keyword_while:
            getsym(str, pos, len);
            if( exp(str, pos, len) ) {
                if( t == lex_token::keyword_do ) {
                    getsym(str, pos, len);
                    if( block(str, pos, len) ) {
                        if( t == lex_token::keyword_end ) {
                            getsym(str, pos, len);
                            std::cout << "Exiting STATEMENT." << std::endl;
                            return true;
                        }
                    }
                }
            }
            std::cout << "Exiting STATEMENT." << std::endl;
            return false;
        case lex_token::keyword_repeat:
            getsym(str, pos, len);
            if( block(str, pos, len) ) {
                if( t == lex_token::keyword_until ) {
                    getsym(str, pos, len);
                    std::cout << "Exiting STATEMENT - Entering EXP." << std::endl;
                    return exp(str, pos, len);
                }
            }
            std::cout << "Exiting STATEMENT." << std::endl;
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
                                        std::cout << "Exiting STATEMENT." << std::endl;
                                        return false;
                                    }
                                } else {
                                    std::cout << "Exiting STATEMENT." << std::endl;
                                    return false;
                                }
                            } else {
                                std::cout << "Exiting STATEMENT." << std::endl;
                                return false;
                            }
                        }
                        if( t == lex_token::keyword_else ) {
                            getsym(str, pos, len);
                            if( block(str, pos, len) ) {
                                // do whatever
                            } else {
                                std::cout << "Exiting STATEMENT." << std::endl;
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
            std::cout << "Exiting STATEMENT." << std::endl;
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
                                        std::cout << "Exiting STATEMENT." << std::endl;
                                        return false;
                                    }
                                }
                                if( t == lex_token::keyword_do ) {
                                    getsym(str, pos, len);
                                    if( block(str, pos, len) ) {
                                        if( t == lex_token::keyword_end ) {
                                            getsym(str, pos, len);
                                            std::cout << "Exiting STATEMENT." << std::endl;
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
                                    std::cout << "Exiting STATEMENT." << std::endl;
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
            std::cout << "Exiting STATEMENT." << std::endl;
            return false;
        case lex_token::keyword_function:
            getsym(str, pos, len);
            if( funcname(str, pos, len) ) {
                std::cout << "Exiting STATEMENT - Entering FUNCBODY." << std::endl;
                return funcbody(str, pos, len);
            }
            std::cout << "Exiting STATEMENT." << std::endl;
            return false;
        case lex_token::keyword_local:
            getsym(str, pos, len);
            if( t == lex_token::keyword_function ) {
                getsym(str, pos, len);
                if( t == lex_token::identifier ) {
                    getsym(str, pos, len);
                    std::cout << "Exiting STATEMENT - Entering FUNCBODY." << std::endl;
                    return funcbody(str, pos, len);
                }
            } else if ( namelist(str, pos, len) ) {
                if( t == lex_token::oper_assignment ) {
                    getsym(str, pos, len);
                    std::cout << "Exiting STATEMENT - Entering EXPLIST." << std::endl;
                    return explist(str, pos, len);
                }
                std::cout << "Exiting STATEMENT." << std::endl;
                return true;
            }
            std::cout << "Exiting STATEMENT." << std::endl;            
            return false;
        default:
            if( assignment_or_call(str, pos, len) ) {
                // stuff here
                return true;
            }
            /*
            if( functioncall(str, pos, len) ) {
                std::cout << "Exiting STATEMENT." << std::endl;
                return true;
            } else if( varlist(str, pos, len) ) {
                if( t == lex_token::oper_assignment ) {
                    getsym(str, pos, len);
                    std::cout << "Exiting STATEMENT - Entering EXPLIST." << std::endl;
                    return explist(str, pos, len);
                }
            }
            */
            std::cout << "Exiting STATEMENT." << std::endl;
            return false;
    }
}

bool laststat(const char* str, int *pos, int len) {
    std::cout << "Entering LAST-STATEMENT." << std::endl;
    if( t == lex_token::keyword_return ) {
        getsym(str, pos, len);
        if( explist(str, pos, len) ) {
            ; // do whatever
        }
        std::cout << "Exiting LAST-STATEMENT." << std::endl;
        return true;
    } else if( t == lex_token::keyword_break ) {
        getsym(str, pos, len);
        std::cout << "Exiting LAST-STATEMENT." << std::endl;
        return true;
    }
    std::cout << "Exiting LAST-STATEMENT." << std::endl;
    return false;
}

bool funcname(const char* str, int *pos, int len) {
    std::cout << "Entering FUNCNAME." << std::endl;
    if( t == lex_token::identifier ) {
        getsym(str, pos, len);
        while( t == lex_token::period ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier ) {
                getsym(str, pos, len);
            } else {
                std::cout << "Exiting FUNCNAME." << std::endl;
                return false;
            }
        }
        if( t == lex_token::colon ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier ) {
                // okay, do stuff here
                getsym(str, pos, len);
            } else {
                std::cout << "Exiting FUNCNAME." << std::endl;
                return false;
            }
        }
        std::cout << "Exiting FUNCNAME." << std::endl;
        return true;
    }
    std::cout << "Exiting FUNCNAME." << std::endl;
    return false;
}

bool varlist(const char* str, int *pos, int len) {
    std::cout << "Entering VARLIST." << std::endl;
    if( var(str, pos, len) ) {
        while( t == lex_token::comma ) {
            var(str, pos, len);
            getsym(str, pos, len);
        }
        std::cout << "Exiting VARLIST." << std::endl;
        return true;
    }
    std::cout << "Exiting VARLIST." << std::endl;
    return false;
}

/*

bool var(const char* str, int *pos, int len) {
    std::cout << "Entering VAR." << std::endl;
    if( t == lex_token::identifier || functioncall(str, pos, len) || t == lex_token::open_paren ) {
        if( t == lex_token::open_paren ) {
            getsym(str, pos, len);
            if( exp(str, pos, len) ) {
                if( t == lex_token::close_paren ) {
                    getsym(str, pos, len);
                } else {
                    std::cout << "Exiting VAR." << std::endl;
                    return false;
                }
            } else {
                std::cout << "Exiting VAR." << std::endl;
                return false;
            }
        } else if( t == lex_token::identifier ) {
            getsym(str, pos, len);
        }
        while( t == lex_token::open_squarebracket || t == lex_token::period ) {
            if( t == lex_token::open_squarebracket ) {
                getsym(str, pos, len);
                if( exp(str, pos, len) ) {
                    if( t == lex_token::close_squarebracket ) {
                        getsym(str, pos, len);
                    } else {
                        std::cout << "Exiting VAR." << std::endl;
                        return false;
                    }
                } else {
                    std::cout << "Exiting VAR." << std::endl;
                    return false;
                }
            } else if (t == lex_token::period) {
                getsym(str, pos, len);
                if( t == lex_token::identifier ) {
                    getsym(str, pos, len);
                } else {
                    std::cout << "Exiting VAR." << std::endl;
                    return false;
                }
            }
        }
        std::cout << "Exiting VAR." << std::endl;
        return true;
    }
    std::cout << "Exiting VAR." << std::endl;
    return false;
}

*/

bool var(const char* str, int *pos, int len) {
    std::cout << "Entering VAR." << std::endl;
    if( t == lex_token::identifier ) {
        getsym(str, pos, len);
        std::cout << "Exiting VAR." << std::endl;
        return true;
    } else if( prefixexp(str, pos, len) ) {
        if( t == lex_token::open_squarebracket ) {
            getsym(str, pos, len);
            if( exp(str, pos, len) ) {
                if( t == lex_token::close_squarebracket ) {
                    getsym(str, pos, len);
                    std::cout << "Exiting VAR." << std::endl;
                    return true;
                }
            }
        } else if( t == lex_token::period ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier ) {
                getsym(str, pos, len);
                std::cout << "Exiting VAR." << std::endl;
                return true;
            }
        }
    }
    std::cout << "Exiting VAR." << std::endl;
    return false;
}

bool namelist(const char* str, int *pos, int len) {
    std::cout << "Entering NAMELIST." << std::endl;
    if( exp(str, pos, len) ) {
        while( t == lex_token::comma ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier) {
                // t is now an identifier.
                getsym(str, pos, len);
            } else {
                std::cout << "Exiting NAMELIST." << std::endl;
                return false;
            }
        }
        std::cout << "Exiting NAMELIST." << std::endl;
        return true;
    }
    std::cout << "Exiting NAMELIST." << std::endl;
    return false;
}

bool explist(const char* str, int *pos, int len) {
    std::cout << "Entering EXPLIST." << std::endl;
    if( exp(str, pos, len) ) {
        while( t == lex_token::comma ) {
            exp(str, pos, len);
            getsym(str, pos, len);
        }
        std::cout << "Exiting EXPLIST." << std::endl;
        return true;
    }
    std::cout << "Exiting EXPLIST." << std::endl;
    return false;
}

bool exp(const char* str, int *pos, int len) {
    std::cout << "Entering EXP." << std::endl;
    if( t == lex_token::value_nil || t == lex_token::value_false || t == lex_token::value_true || t == lex_token::numeric_literal || t == lex_token::string || t == lex_token::ellipsis ) {
        getsym(str, pos, len);
        std::cout << "Exiting EXP." << std::endl;
        return true;
    } else if( function(str, pos, len) ) {
        std::cout << "Exiting EXP." << std::endl;
        return true;
    } else if( prefixexp(str, pos, len) ) {
        std::cout << "Exiting EXP." << std::endl;
        return true;
    } else if( tableconstructor(str, pos, len) ) {
        std::cout << "Exiting EXP." << std::endl;
        return true;
    } else if( exp(str, pos, len) ) {
        if( binop(str, pos, len) ) {
            if( exp(str, pos, len) ) {
                std::cout << "Exiting EXP." << std::endl;
                return true;
            }
        }
    } else if( unop(str, pos, len) ) {
        std::cout << "Exiting EXP." << std::endl;
        return true;
    }
    std::cout << "Exiting EXP." << std::endl;
    return false;
}

bool primaryexp(const char* str, int *pos, int len) {
    std::cout << "Entering PRIMARYEXP." << std::endl;
    if( prefixexp(str, pos, len) ) {
        bool found_args = false;
        while( t == lex_token::period || t == lex_token::colon || (found_args = args(str, pos, len))  ) {
            if( !found_args ) {
                getsym(str, pos, len);
                // do stuff here
            } else {
                found_args = false;
                // do stuff here
            }
        }
        std::cout << "Exiting PRIMARYEXP." << std::endl;
        return true;
    }
    std::cout << "Exiting PRIMARYEXP." << std::endl;
    return false;
}

bool assignment_or_call(const char* str, int *pos, int len) {
    std::cout << "Entering ASSIGNMENT-OR-CALL." << std::endl;
    if( primaryexp(str, pos, len) ) {
        if( t == lex_token::comma ) {
            getsym(str, pos, len);
            // multiple assignment
        } else if(t == lex_token::oper_assignment) {
            getsym(str, pos, len);
            // single assignment;
        } else {
            // function call
            ;
        }
        std::cout << "Exiting ASSIGNMENT-OR-CALL." << std::endl;
        return true;
    }
    std::cout << "Exiting ASSIGNMENT-OR-CALL." << std::endl;
    return false;
}

bool prefixexp(const char* str, int *pos, int len) {
    recursion_depth++;
    assert( recursion_depth < 10 );
    std::cout << "Entering PREFIXEXP." << std::endl;
    if( var(str, pos, len) ) {
        std::cout << "Exiting PREFIXEXP." << std::endl;
        return true;
    /*} else if( functioncall(str, pos, len) ) {
        std::cout << "Exiting PREFIXEXP." << std::endl;
        return true; */
    } else if ( t == lex_token::open_paren ) {
        getsym(str, pos, len);
        if( exp(str, pos, len) ) {
            if( t == lex_token::close_paren ) {
                getsym(str, pos, len);
                std::cout << "Exiting PREFIXEXP." << std::endl;
                return true;
            }
        }
    }
    std::cout << "Exiting PREFIXEXP." << std::endl;
    return false;
}

/*
bool functioncall(const char* str, int *pos, int len) {
    std::cout << "Entering FUNCTIONCALL." << std::endl;
    while(  )
    std::cout << "Exiting FUNCTIONCALL." << std::endl;
    return false;
}
*/

bool functioncall(const char* str, int *pos, int len) {
    std::cout << "Entering FUNCTIONCALL." << std::endl;
    if( prefixexp(str, pos, len) ) {
        if( t == lex_token::colon ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier ) {
                getsym(str, pos, len);
                ; // do stuff here
            }
        }
        if( args(str, pos, len) ) {
            std::cout << "Exiting FUNCTIONCALL." << std::endl;
            return true;
        }
    }
    std::cout << "Exiting FUNCTIONCALL." << std::endl;
    return false;
}

bool args(const char* str, int *pos, int len) {
    std::cout << "Entering ARGS." << std::endl;
    if( t == lex_token::open_paren ) {
        getsym(str, pos, len);
        if( explist(str, pos, len) ) {
            ; // do stuff here
        }
        if( t == lex_token::close_paren ) {
            getsym(str, pos, len);
            std::cout << "Exiting ARGS." << std::endl;
            return true;
        }
    } else if( tableconstructor(str, pos, len) ) {
        getsym(str, pos, len);
        std::cout << "Exiting ARGS." << std::endl;
        return true;
    } else if( t == lex_token::string ) {
        getsym(str, pos, len);
        std::cout << "Exiting ARGS." << std::endl;
        return true;
    }
    std::cout << "Exiting ARGS." << std::endl;
    return false;
}

bool function(const char* str, int *pos, int len) {
    std::cout << "Entering FUNCTION." << std::endl;
    if( t == lex_token::keyword_function ) {
        getsym(str, pos, len);
        std::cout << "Exiting FUNCTION - Entering FUNCBODY." << std::endl;
        return funcbody(str, pos, len);
    }
    std::cout << "Exiting FUNCTION." << std::endl;
    return false;
}

bool funcbody(const char* str, int *pos, int len) {
    std::cout << "Entering FUNCBODY." << std::endl;
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
                    std::cout << "Exiting FUNCBODY." << std::endl;
                    return true;
                }
            }
        }
    }
    std::cout << "Exiting FUNCBODY." << std::endl;
    return false;
}

bool parlist(const char* str, int *pos, int len) {
    std::cout << "Entering PARLIST." << std::endl;
    if( t == lex_token::ellipsis ) {
        getsym(str, pos, len);
        std::cout << "Exiting PARLIST." << std::endl;
        return true;
    } else if( namelist(str, pos, len) ) {
        if( t == lex_token::comma ) {
            getsym(str, pos, len);
            if( t == lex_token::ellipsis ) {
                getsym(str, pos, len);
                std::cout << "Exiting PARLIST." << std::endl;
                return true;
            }
        }
    }
    std::cout << "Exiting PARLIST." << std::endl;
    return false;
}

bool tableconstructor(const char* str, int *pos, int len) {
    std::cout << "Entering TABLECONSTRUCTOR." << std::endl;
    if( t == lex_token::open_curlybrace ) { 
        getsym(str, pos, len);
        fieldlist(str, pos, len); // optional element
        if( t == lex_token::close_curlybrace ) {
            getsym(str, pos, len);
            std::cout << "Exiting TABLECONSTRUCTOR." << std::endl;
            return true;
        }
    }
    std::cout << "Exiting TABLECONSTRUCTOR." << std::endl;
    return false;
}

bool fieldlist(const char* str, int *pos, int len) {
    std::cout << "Entering FIELDLIST." << std::endl;
    if( field(str, pos, len) ) {
        getsym(str, pos, len);
        while( fieldsep(str, pos, len) ) { // is the current character a field separator?
            field(str, pos, len); // let's hope the next character(s) are a field.
            getsym(str, pos, len);
        }
        if( fieldsep(str, pos, len) ) {
            ; // stuff here
        }
        std::cout << "Exiting FIELDLIST." << std::endl;
        return true;
    }
    std::cout << "Exiting FIELDLIST." << std::endl;
    return false;
}

bool field(const char* str, int *pos, int len) {
    std::cout << "Entering FIELD." << std::endl;
    if( t == lex_token::open_squarebracket ){
        getsym(str, pos, len);
        if(exp(str, pos, len)) {
            if( t == lex_token::close_squarebracket ) {
                getsym(str, pos, len);
                if( t == lex_token::oper_assignment  ) {
                    getsym(str, pos, len);
                    std::cout << "Exiting FIELD - Entering EXP." << std::endl;
                    return exp(str, pos, len);
                }
            }
            std::cout << "Exiting FIELD." << std::endl;
            return false;
        }
    } else {    
        if(exp(str, pos, len)) {
            if(t == lex_token::oper_assignment) {
                getsym(str, pos, len);
                std::cout << "Exiting FIELD - Entering EXP." << std::endl;
                return exp(str, pos, len);
            }
            getsym(str, pos, len);
            std::cout << "Exiting FIELD." << std::endl;
            return true;
        }
    }
    std::cout << "Exiting FIELD." << std::endl;
    return false;
}

bool fieldsep(const char* str, int *pos, int len) {
    std::cout << "Entering FIELDSEP." << std::endl;
    if(t == lex_token::comma) {
        getsym(str, pos, len);
        std::cout << "Exiting FIELDSEP." << std::endl;
        return true;
    } else if(t == lex_token::semicolon) {
        getsym(str, pos, len);
        std::cout << "Exiting FIELDSEP." << std::endl;
        return true;
    }
    std::cout << "Exiting FIELDSEP." << std::endl;
    return false;
}

bool binop(const char* str, int *pos, int len) {
    std::cout << "Entering BINOP." << std::endl;
    if(t == lex_token::oper_plus || t == lex_token::oper_minus || t == lex_token::oper_multiply || t == lex_token::oper_divide || t == lex_token::oper_exp || t == lex_token::oper_modulus || t == lex_token::cmp_less || t == lex_token::cmp_greater || t == lex_token::cmp_less_or_equal || t == lex_token::cmp_greater_or_equal || t == lex_token::cmp_equal || t == lex_token::cmp_unequal || t == lex_token::oper_and || t == lex_token::oper_or) {
        // do stuff here -- we've found a binary operator.
        getsym(str, pos, len);
        std::cout << "Exiting BINOP." << std::endl;
        return true;
    }
    std::cout << "Exiting BINOP." << std::endl;
    return false;
}

bool unop(const char* str, int *pos, int len) {
    std::cout << "Entering UNOP." << std::endl;
    if(t == lex_token::oper_inverse || t == lex_token::oper_not || t == lex_token::oper_len) {
        // do stuff here -- we've found an unary operator.
        getsym(str, pos, len);
        std::cout << "Exiting UNOP." << std::endl;
        return true;
    }
    std::cout << "Exiting UNOP." << std::endl;
    return false;
}

//const char* test_str = "if (t >= -5) and (#z ~= 51515) then a = b.some_function(\"this is a string\", [[this is a string \"with quotes\" in it.]]) elseif t < 5 then f2 = f.another_function(52, 33) end";
//const char* test_str = "if (a > 5) then a_func(ident) end";
const char* test_str = "if b == 10 then a = 5 end";

int main() {
    std::string s(test_str);
    std::cout << s << std::endl;
    std::cout << "Lexing input. (len=" << s.length() << ")" << std::endl;
    lex( s );
    
    int pos = 0;
    int len = s.length()+1;
    char* test = NULL;
    
    // "preload" the symbol variables
    std::cout << "Lexer run complete, loading parser." << std::endl;
    std::cout << test_str << std::endl;
    //std::cout << token_to_str( lex_next(test_str, &pos, len, &test ) ) << std::endl;
    getsym(test_str, &pos, len);
    
    std::cout << "First symbol: " << token_to_str( t ) << std::endl;
    
    // go
    std::cout << "Running parser." << std::endl;
    chunk(test_str, &pos, len);
    return 0;
}