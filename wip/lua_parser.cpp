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
bool data_var(const char*, int*, int);
bool climbing_prefix(const char*, int*, int, int);
bool precedence_climbing(const char*, int*, int, int);
int get_oper_precedence(lex_token);

void getsym(const char* str, int *pos, int len) {
    if(t == lex_token::identifier || t == lex_token::numeric_literal || t == lex_token::string) {
        delete[] t_str;
    }
    
    lex_token t_candidate = lex_next(str, pos, len, &t_str);
    while( t_candidate == lex_token::whitespace ) {
        t_candidate = lex_next(str, pos, len, &t_str);
    }
    //std::cout << "T-candidate @ " << *pos <<  ": " << token_to_str( t_candidate ) << std::endl;
    t = t_candidate;
}

lex_token lookahead(const char* str, int len, int *cur_pos, int offset, char* returned_str) { // get the token [offset] tokens away from [*cur_pos]
    int pos = *cur_pos;
    lex_token token = lex_token::nothing;
    for(int i=0;i<offset;i++) {
        if( pos < len ) {
            break;
        }
        token = lex_next( str, &pos, len, &returned_str );
    }
    return token;
}

void error( const char* error_str, int pos ) {
    std::cout << "Error at position " << pos << ":" << std::endl;
    std::cout << error_str << std::endl;
}

bool chunk(const char* str, int *pos, int len) {
    //std::cout << "Entering CHUNK." << std::endl;
    while( stat(str, pos, len) ) {
        if( t == lex_token::semicolon ) {
            getsym(str, pos, len);
        }
    }
    if( laststat(str, pos, len) ) {
        if( t == lex_token::semicolon ) {
            getsym(str, pos, len);
        }
    }
    //std::cout << "Exiting CHUNK successfully." << std::endl;
    return true;
}

bool block(const char* str, int *pos, int len) {
    return chunk(str, pos, len);
}

bool stat(const char* str, int *pos, int len) {
    //std::cout << "Entering STATEMENT." << std::endl;
    switch(t) {
        case lex_token::keyword_do:
            //std::cout << "STATEMENT: Do case." << std::endl;
            getsym(str, pos, len);
            if( block(str, pos, len) ) {
                if( t == lex_token::keyword_end ) {
                    getsym(str, pos, len);
                    //std::cout << "Exiting STATEMENT - found do block" << std::endl;
                    return true;
                } else {
                    error("expected \"end\" after \"do\"",*pos);
                }
            }
            //std::cout << "Exiting STATEMENT." << std::endl;
            error("expected code block after \"do\"",*pos);
            return false;
        case lex_token::keyword_while:
            getsym(str, pos, len);
            if( exp(str, pos, len) ) {
                if( t == lex_token::keyword_do ) {
                    getsym(str, pos, len);
                    if( block(str, pos, len) ) {
                        if( t == lex_token::keyword_end ) {
                            getsym(str, pos, len);
                            //std::cout << "Exiting STATEMENT - found while loop" << std::endl;
                            return true;
                        } else {
                            error("expected \"end\" to close \"while\"",*pos);
                        }
                    } else {
                        error("expected code block after \"do\"",*pos);
                    }
                } else {
                    error("expected \"do\" after \"while\"",*pos);
                }
            }
            error("expected condition after \"while\"",*pos);
            //std::cout << "Exiting STATEMENT." << std::endl;
            return false;
        case lex_token::keyword_repeat:
            //std::cout << "STATEMENT: Repeat case." << std::endl;
            getsym(str, pos, len);
            if( block(str, pos, len) ) {
                if( t == lex_token::keyword_until ) {
                    getsym(str, pos, len);
                    //std::cout << "Exiting STATEMENT - Entering EXP - found repeat-until block" << std::endl;
                    return exp(str, pos, len);
                } else {
                    error("expected \"until\" after \"repeat\"",*pos);
                }
            }
            error("expected code block after \"repeat\"",*pos);
            //std::cout << "Exiting STATEMENT." << std::endl;
            return false;
        case lex_token::keyword_if:
            //std::cout << "STATEMENT: If case." << std::endl;
            getsym(str, pos, len);
            //std::cout << "found if" << std::endl;
            if( exp(str, pos, len) ) {
                //std::cout << "found conditional" << std::endl;
                if( t == lex_token::keyword_then ) {
                    //std::cout << "found then" << std::endl;
                    getsym(str, pos, len);
                    if( block(str, pos, len) ) {
                        //std::cout << "found then block" << std::endl;
                        while( t == lex_token::keyword_elseif ) {
                            getsym(str, pos, len);
                            if( exp(str, pos, len) ) {
                                if( t == lex_token::keyword_then ) {
                                    getsym(str, pos, len);
                                    //std::cout << "found elseif block" << std::endl;
                                    if( block(str, pos, len) ) {
                                        ;// do whatever
                                    } else {
                                        //std::cout << "Exiting STATEMENT." << std::endl;
                                        error("expected code block after \"then\"",*pos);
                                        return false;
                                    }
                                } else {
                                    //std::cout << "Exiting STATEMENT." << std::endl;
                                    error("expected \"then\" after \"elseif\"",*pos);
                                    return false;
                                }
                            } else {
                                //std::cout << "Exiting STATEMENT." << std::endl;
                                error("expected condition after \"elseif\"",*pos);
                                return false;
                            }
                        }
                        if( t == lex_token::keyword_else ) {
                            getsym(str, pos, len);
                            //std::cout << "found else block" << std::endl;
                            if( block(str, pos, len) ) {
                                // do whatever
                            } else {
                                //std::cout << "Exiting STATEMENT." << std::endl;
                                error("expected code block after \"else\"",*pos);
                                return false;
                            }
                        }
                        if( t == lex_token::keyword_end ) {
                            getsym(str, pos, len);
                            //std::cout << "Exiting STATEMENT - found if-then block" << std::endl;
                            return true;
                        } else {
                            error("expected \"end\" to close \"if\" block",*pos);
                        }
                    } else {
                        error("expected code block after \"then\"",*pos);
                    }
                } else {
                    error("expected \"then\" after \"if\"",*pos);
                }
            } else {
                error("expected condition after \"if\"",*pos);
            }
            //std::cout << "Exiting STATEMENT." << std::endl;
            return false;
        case lex_token::keyword_for:
            //std::cout << "STATEMENT: For case." << std::endl;
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
                                        ; // do whatever -- step expression
                                    } else {
                                        //std::cout << "Exiting STATEMENT." << std::endl;
                                        error("expected step expression in numeric \"for\"",*pos);
                                        return false;
                                    }
                                }
                                if( t == lex_token::keyword_do ) {
                                    getsym(str, pos, len);
                                    if( block(str, pos, len) ) {
                                        if( t == lex_token::keyword_end ) {
                                            getsym(str, pos, len);
                                            //std::cout << "Exiting STATEMENT - found numeric for block" << std::endl;
                                            return true;
                                        } else {
                                            error("expected \"end\" after \"for\" block",*pos);
                                        }
                                    } else {
                                        error("expected code block after \"do\"",*pos);
                                    }
                                } else {
                                    error("expected \"do\" in numeric \"for\"",*pos);
                                }
                            } else {
                                error("expected end expression in numeric \"for\"",*pos);
                            }
                        } else {
                            error("expected comma after start expression in numeric \"for\"",*pos);
                        }
                    } else {
                        error("expected start expression after assignment in numeric \"for\"",*pos);
                    }
                } else {
                    error("expected assignment operator after identifier in numeric \"for\"",*pos);
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
                                    //std::cout << "Exiting STATEMENT - found iterator for block" << std::endl;
                                    return true;
                                } else {
                                    error("expected \"end\" after iterator \"for\"",*pos);
                                }
                            } else {
                                error("expected code block after iterator \"for\"",*pos);
                            }
                        } else {
                            error("expected \"do\" after expression list in iterator \"for\"",*pos);
                        }
                    } else {
                        error("expected expression list after \"in\" in iterator \"for\"",*pos);
                    }
                } else {
                    error("expected \"in\" after identifier list in iterator \"for\"",*pos);
                }
            }
            error("expected namelist or identifier after \"for\"",*pos);
            //std::cout << "Exiting STATEMENT." << std::endl;
            return false;
        case lex_token::keyword_function:
            //std::cout << "STATEMENT: function case." << std::endl;
            getsym(str, pos, len);
            if( funcname(str, pos, len) ) {
                //std::cout << "Exiting STATEMENT - Entering FUNCBODY - found nonlocal function definition" << std::endl;
                return funcbody(str, pos, len);
            }
            error("expected identifier after \"function\"",*pos);
            //std::cout << "Exiting STATEMENT." << std::endl;
            return false;
        case lex_token::keyword_local:
            //std::cout << "STATEMENT: local case." << std::endl;
            getsym(str, pos, len);
            if( t == lex_token::keyword_function ) {
                getsym(str, pos, len);
                if( t == lex_token::identifier ) {
                    getsym(str, pos, len);
                    //std::cout << "Exiting STATEMENT - Entering FUNCBODY - found local function definition" << std::endl;
                    return funcbody(str, pos, len);
                } else {
                    error("expected identifier after \"local function\"",*pos);
                }
            } else if ( namelist(str, pos, len) ) {
                if( t == lex_token::oper_assignment ) {
                    getsym(str, pos, len);
                    //std::cout << "Exiting STATEMENT - Entering EXPLIST - found local variable assignment" << std::endl;
                    return explist(str, pos, len);
                }
                //std::cout << "Exiting STATEMENT - found local variable definition" << std::endl;
                return true;
            }
            error("expected \"function\" or identifiers after \"local\"",*pos);
            //std::cout << "Exiting STATEMENT." << std::endl;
            return false;
        default:
            //std::cout << "STATEMENT: Default case." << std::endl;
            if( assignment_or_call(str, pos, len) ) {
                // stuff here
                //std::cout << "Exiting STATEMENT - found assignment / call" << std::endl;
                return true;
            }
            //std::cout << "Exiting STATEMENT." << std::endl;
            return false;
    }
    return false;
}

bool laststat(const char* str, int *pos, int len) {
    //std::cout << "Entering LAST-STATEMENT." << std::endl;
    if( t == lex_token::keyword_return ) {
        getsym(str, pos, len);
        if( explist(str, pos, len) ) {
            ; // do whatever
        }
        //std::cout << "Exiting LAST-STATEMENT - found return" << std::endl;
        return true;
    } else if( t == lex_token::keyword_break ) {
        getsym(str, pos, len);
        //std::cout << "Exiting LAST-STATEMENT - found break" << std::endl;
        return true;
    }
    //std::cout << "Exiting LAST-STATEMENT." << std::endl;
    return false;
}

bool funcname(const char* str, int *pos, int len) {
    //std::cout << "Entering FUNCNAME." << std::endl;
    if( t == lex_token::identifier ) {
        getsym(str, pos, len);
        while( t == lex_token::period ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier ) {
                getsym(str, pos, len);
            } else {
                //std::cout << "Exiting FUNCNAME." << std::endl;
                error("expected identifier after period in function name",*pos);
                return false;
            }
        }
        if( t == lex_token::colon ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier ) {
                // okay, do stuff here
                getsym(str, pos, len);
            } else {
                //std::cout << "Exiting FUNCNAME." << std::endl;
                error("expected identifier after colon in function name",*pos);
                return false;
            }
        }
        //std::cout << "Exiting FUNCNAME - found identifier(s)" << std::endl;
        return true;
    }
    //std::cout << "Exiting FUNCNAME." << std::endl;
    return false;
}

bool varlist(const char* str, int *pos, int len) {
    //std::cout << "Entering VARLIST." << std::endl;
    if( var(str, pos, len) ) {
        while( t == lex_token::comma ) {
            var(str, pos, len);
            getsym(str, pos, len);
        }
        //std::cout << "Exiting VARLIST - found variables" << std::endl;
        return true;
    }
    //std::cout << "Exiting VARLIST." << std::endl;
    return false;
}

bool var(const char* str, int *pos, int len) {
    //std::cout << "Entering VAR." << std::endl;
    if( t == lex_token::identifier ) {
        //std::cout << "Exiting VAR - found simple identifier (" << t_str << ")" << std::endl;
        getsym(str, pos, len);
        return true;
    } else if( prefixexp(str, pos, len) ) {
        if( t == lex_token::open_squarebracket ) {
            getsym(str, pos, len);
            if( exp(str, pos, len) ) {
                if( t == lex_token::close_squarebracket ) {
                    getsym(str, pos, len);
                    //std::cout << "Exiting VAR - found var in array index" << std::endl;
                    return true;
                } else {
                    error("expected [ to close ]",*pos);
                }
            } else {
                error("expected expression after [",*pos);
            }
        } else if( t == lex_token::period ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier ) {
                getsym(str, pos, len);
                //std::cout << "Exiting VAR - found dot-name" << std::endl;
                return true;
            } else {
                error("expected identifier after period",*pos);
            }
        } else {
            error("expected [ or period after prefix-expression",*pos);
        }
    }
    //std::cout << "Exiting VAR." << std::endl;
    return false;
}

bool namelist(const char* str, int *pos, int len) {
    //std::cout << "Entering NAMELIST." << std::endl;
    if( exp(str, pos, len) ) {
        while( t == lex_token::comma ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier) {
                // t is now an identifier.
                getsym(str, pos, len);
            } else {
                //std::cout << "Exiting NAMELIST." << std::endl;
                error("expected \"\" after \"\"",*pos);
                return false;
            }
        }
        //std::cout << "Exiting NAMELIST - found list of names" << std::endl;
        return true;
    }
    //std::cout << "Exiting NAMELIST." << std::endl;
    return false;
}

bool explist(const char* str, int *pos, int len) {
    //std::cout << "Entering EXPLIST." << std::endl;
    if( exp(str, pos, len) ) {
        while( t == lex_token::comma ) {
            getsym(str, pos, len);
            exp(str, pos, len);
        }
        //std::cout << "Exiting EXPLIST - found list of expressions" << std::endl;
        return true;
    }
    //std::cout << "Exiting EXPLIST." << std::endl;
    return false;
}

bool data_var(const char* str, int *pos, int len) {
    if( t == lex_token::value_nil || t == lex_token::value_true || t == lex_token::value_false || t == lex_token::identifier || t == lex_token::string || t == lex_token::numeric_literal || tableconstructor(str, pos, len) ) {
        return true;
    }
    return false;
}

bool climbing_prefix(const char* str, int *pos, int len, int level) { // P
    //std::cout << "Entering CLIMBING_PREFIX." << std::endl;
    if( unop(str, pos, len) ) {
        //std::cout << "Found unary operator: " << token_to_str( t ) << std::endl;
        getsym(str, pos, len);
        return precedence_climbing(str, pos, len, level);
    } else if( t == lex_token::open_paren ) {
        getsym(str, pos, len);
        //std::cout << "Recursing to exp(0)." << std::endl;
        if( exp(str, pos, len) ) {
            if( t == lex_token::close_paren ) {
                getsym(str, pos, len);
                return true;
            } else {
                error("expected ) to close (",*pos);
            }
        } else {
            error("expected expression after (",*pos);
        }
    } else if( data_var(str, pos, len) ) {
        //std::cout << "Found data identifier: " << token_to_str( t ) << std::endl;
        getsym(str, pos, len);
        return true;
    }
    //std::cout << "Exiting CLIMBING_PREFIX." << std::endl;
    return false;
}

bool precedence_climbing(const char* str, int *pos, int len, int level) { // Exp(p)
    //std::cout << "Entering PRECEDENCE_CLIMBING" << std::endl;
    if( climbing_prefix(str, pos, len, level) ) {
        while( binop(str, pos, len) && get_oper_precedence(t) >= level  ) {
            lex_token t_saved = t;
            //std::cout << "Found operator, type: " << token_to_str( t_saved ) << std::endl;
            if( t == lex_token::oper_exp ) {
                getsym(str, pos, len);
                //std::cout << "Recursing, right-associative." << std::endl;
                if( precedence_climbing(str, pos, len, get_oper_precedence(t_saved)+1) ) {
                    ;
                }
            } else {
                getsym(str, pos, len);
                //std::cout << "Recursing, left-associative." << std::endl;
                if( precedence_climbing(str, pos, len, get_oper_precedence(t_saved)) ) {
                    ;
                }
            }
        }
        //std::cout << "Exiting PRECEDENCE_CLIMBING successfully." << std::endl;
        return true;
    }
    //std::cout << "Exiting PRECEDENCE_CLIMBING" << std::endl;
    return false;
}

int get_oper_precedence(lex_token token) {
    switch(token) {
        case lex_token::oper_or:
            return 1;
        case lex_token::oper_and:
            return 2;
        case lex_token::cmp_less:
        case lex_token::cmp_greater:
        case lex_token::cmp_less_or_equal:
        case lex_token::cmp_greater_or_equal:
        case lex_token::cmp_equal:
        case lex_token::cmp_unequal:
            return 3;
        case lex_token::ellipsis:
            return 4;
        case lex_token::oper_plus:
        case lex_token::oper_minus:
            return 5;
        case lex_token::oper_multiply:
        case lex_token::oper_divide:
        case lex_token::oper_modulus:
            return 6;
        case lex_token::oper_not:
        case lex_token::oper_len:
        case lex_token::oper_inverse:
            return 7;
        case lex_token::oper_exp:
            return 8;
    }
}

bool binop(const char* str, int *pos, int len) {
    //std::cout << "Entering BINOP." << std::endl;
    if(t == lex_token::oper_plus || t == lex_token::oper_minus || t == lex_token::oper_multiply || t == lex_token::oper_divide || t == lex_token::oper_exp || t == lex_token::oper_modulus || t == lex_token::cmp_less || t == lex_token::cmp_greater || t == lex_token::cmp_less_or_equal || t == lex_token::cmp_greater_or_equal || t == lex_token::cmp_equal || t == lex_token::cmp_unequal || t == lex_token::oper_and || t == lex_token::oper_or) {
        // do stuff here -- we've found a binary operator.
        //getsym(str, pos, len);
        //std::cout << "Exiting BINOP - found one" << std::endl;
        return true;
    }
    //std::cout << "Exiting BINOP." << std::endl;
    return false;
}

bool unop(const char* str, int *pos, int len) {
    //std::cout << "Entering UNOP." << std::endl;
    if(t == lex_token::oper_inverse || t == lex_token::oper_not || t == lex_token::oper_len) {
        // do stuff here -- we've found an unary operator.
        //getsym(str, pos, len);
        //std::cout << "Exiting UNOP - found one" << std::endl;
        return true;
    }
    //std::cout << "Exiting UNOP." << std::endl;
    return false;
}


bool exp(const char* str, int *pos, int len) {
    //std::cout << "Entering EXP." << std::endl;
    return precedence_climbing(str, pos, len, 0);
}

bool primaryexp(const char* str, int *pos, int len) {
    //std::cout << "Entering PRIMARYEXP." << std::endl;
    if( prefixexp(str, pos, len) ) {
        bool found_args = args(str, pos, len);
        while( found_args || t == lex_token::period || t == lex_token::colon ){
            if( t == lex_token::period || t == lex_token::colon ) {
                getsym(str, pos, len);
                // do stuff here
            } else if(found_args) {
                found_args = false;
                // do stuff here
            }
            found_args = args(str, pos, len);
        }
        //std::cout << "Exiting PRIMARYEXP - found prefixexp" << std::endl;
        return true;
    }
    //std::cout << "Exiting PRIMARYEXP." << std::endl;
    return false;
}

bool assignment_or_call(const char* str, int *pos, int len) {
    //std::cout << "Entering ASSIGNMENT-OR-CALL." << std::endl;
    if( primaryexp(str, pos, len) ) {
        if( t == lex_token::comma ) {
            getsym(str, pos, len);
            // multiple assignment
            //std::cout << "ASSIGNMENT-OR-CALL: found multiple assignment." << std::endl;
        } else if(t == lex_token::oper_assignment) {
            getsym(str, pos, len);
            // single assignment;
            //std::cout << "ASSIGNMENT-OR-CALL: found single assignment." << std::endl;
            if( exp(str, pos, len) ) {
                ; // stuff
            } else {
                error("expected expression after single assignment",*pos);
            }
        } else {
            // function call
            //std::cout << "ASSIGNMENT-OR-CALL: found function call." << std::endl;
        }
        //std::cout << "Exiting ASSIGNMENT-OR-CALL." << std::endl;
        return true;
    }
    //std::cout << "Exiting ASSIGNMENT-OR-CALL." << std::endl;
    return false;
}

bool prefixexp(const char* str, int *pos, int len) {
    recursion_depth++;
    assert( recursion_depth < 10 );
    //std::cout << "Entering PREFIXEXP." << std::endl;
    if( t == lex_token::identifier ) {
        getsym(str, pos, len);
        //std::cout << "Exiting PREFIXEXP - found identifier" << std::endl;
        return true;
    /*} else if( functioncall(str, pos, len) ) {
        //std::cout << "Exiting PREFIXEXP." << std::endl;
        return true; */
    } else if ( t == lex_token::open_paren ) {
        getsym(str, pos, len);
        if( exp(str, pos, len) ) {
            if( t == lex_token::close_paren ) {
                getsym(str, pos, len);
                //std::cout << "Exiting PREFIXEXP - found parenthesed expression" << std::endl;
                return true;
            } else {
                error("expected ) to close (",*pos);
            }
        } else {
            error("expected expression after (",*pos);
        }
    }
    //std::cout << "Exiting PREFIXEXP." << std::endl;
    return false;
}

bool functioncall(const char* str, int *pos, int len) {
    //std::cout << "Entering FUNCTIONCALL." << std::endl;
    if( prefixexp(str, pos, len) ) {
        if( t == lex_token::colon ) {
            getsym(str, pos, len);
            if( t == lex_token::identifier ) {
                getsym(str, pos, len);
                ; // do stuff here
            } else {
                error("expected identifier after period",*pos);
            }
        }
        if( args(str, pos, len) ) {
            //std::cout << "Exiting FUNCTIONCALL - found parameters" << std::endl;
            return true;
        } else {
            error("expected function arguments.",*pos);
        }
    }
    //std::cout << "Exiting FUNCTIONCALL." << std::endl;
    return false;
}

bool args(const char* str, int *pos, int len) {
    //std::cout << "Entering ARGS." << std::endl;
    if( t == lex_token::open_paren ) {
        getsym(str, pos, len);
        if( explist(str, pos, len) ) {
            ; // do stuff here
        }
        if( t == lex_token::close_paren ) {
            getsym(str, pos, len);
            //std::cout << "Exiting ARGS - found close-paren" << std::endl;
            return true;
        } else {
            error("expected ( to close ) in function parameters.",*pos);
        }
    } else if( tableconstructor(str, pos, len) ) {
        getsym(str, pos, len);
        //std::cout << "Exiting ARGS - found constructor" << std::endl;
        return true;
    } else if( t == lex_token::string ) {
        getsym(str, pos, len);
        //std::cout << "Exiting ARGS - found string" << std::endl;
        return true;
    }
    //std::cout << "Exiting ARGS." << std::endl;
    return false;
}

bool function(const char* str, int *pos, int len) {
    //std::cout << "Entering FUNCTION." << std::endl;
    if( t == lex_token::keyword_function ) {
        getsym(str, pos, len);
        //std::cout << "Exiting FUNCTION - Entering FUNCBODY." << std::endl;
        return funcbody(str, pos, len);
    }
    //std::cout << "Exiting FUNCTION." << std::endl;
    return false;
}

bool funcbody(const char* str, int *pos, int len) {
    //std::cout << "Entering FUNCBODY." << std::endl;
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
                    //std::cout << "Exiting FUNCBODY - found function body" << std::endl;
                    return true;
                } else {
                    error("expected \"end\" to close function body.",*pos);
                }
            } else { 
                error("expected code block after function header",*pos);
            }
        } else {
            error("expected ) to close ( in parameter list.",*pos);
        }
    }
    //std::cout << "Exiting FUNCBODY." << std::endl;
    return false;
}

bool parlist(const char* str, int *pos, int len) {
    //std::cout << "Entering PARLIST." << std::endl;
    if( t == lex_token::ellipsis ) {
        getsym(str, pos, len);
        //std::cout << "Exiting PARLIST - found ellipsis" << std::endl;
        return true;
    } else if( namelist(str, pos, len) ) {
        if( t == lex_token::comma ) {
            getsym(str, pos, len);
            if( t == lex_token::ellipsis ) {
                getsym(str, pos, len);
                //std::cout << "Exiting PARLIST - found vararg identifier" << std::endl;
            } else {
                error("errant comma in parameter list",*pos);
            }
        }
        return true;
    }
    //std::cout << "Exiting PARLIST." << std::endl;
    return false;
}

bool tableconstructor(const char* str, int *pos, int len) {
    //std::cout << "Entering TABLECONSTRUCTOR." << std::endl;
    if( t == lex_token::open_curlybrace ) { 
        getsym(str, pos, len);
        fieldlist(str, pos, len); // optional element
        if( t == lex_token::close_curlybrace ) {
            getsym(str, pos, len);
            //std::cout << "Exiting TABLECONSTRUCTOR - found end curly brace" << std::endl;
            return true;
        } else {
            error("expected } to close { in table",*pos);
        }
    }
    //std::cout << "Exiting TABLECONSTRUCTOR." << std::endl;
    return false;
}

bool fieldlist(const char* str, int *pos, int len) {
    //std::cout << "Entering FIELDLIST." << std::endl;
    if( field(str, pos, len) ) {
        getsym(str, pos, len);
        while( fieldsep(str, pos, len) ) { // is the current character a field separator?
            field(str, pos, len); // let's hope the next character(s) are a field.
            getsym(str, pos, len);
        }
        if( fieldsep(str, pos, len) ) {
            ; // stuff here
        }
        //std::cout << "Exiting FIELDLIST - found start-field." << std::endl;
        return true;
    }
    //std::cout << "Exiting FIELDLIST." << std::endl;
    return false;
}

bool field(const char* str, int *pos, int len) {
    //std::cout << "Entering FIELD." << std::endl;
    if( t == lex_token::open_squarebracket ){
        getsym(str, pos, len);
        if(exp(str, pos, len)) {
            if( t == lex_token::close_squarebracket ) {
                getsym(str, pos, len);
                if( t == lex_token::oper_assignment  ) {
                    getsym(str, pos, len);
                    //std::cout << "Exiting FIELD - Entering EXP - found index-assignment." << std::endl;
                    return exp(str, pos, len);
                } else {
                    error("expected assignment after square brackets",*pos);
                }
            } else {
                error("expected ] to close [",*pos);
            }
            //std::cout << "Exiting FIELD." << std::endl;
            return false;
        } else {
            error("expected expression after [",*pos);
        }
    } else if(exp(str, pos, len)) {
        if(t == lex_token::oper_assignment) {
            getsym(str, pos, len);
            //std::cout << "Exiting FIELD - Entering EXP - found expression-assignment" << std::endl;
            return exp(str, pos, len);
        }
        getsym(str, pos, len);
        //std::cout << "Exiting FIELD - found naked expression" << std::endl;
        return true;
    }
    //std::cout << "Exiting FIELD." << std::endl;
    return false;
}

bool fieldsep(const char* str, int *pos, int len) {
    //std::cout << "Entering FIELDSEP." << std::endl;
    if(t == lex_token::comma) {
        getsym(str, pos, len);
        //std::cout << "Exiting FIELDSEP - found comma" << std::endl;
        return true;
    } else if(t == lex_token::semicolon) {
        getsym(str, pos, len);
        //std::cout << "Exiting FIELDSEP - found semicolon" << std::endl;
        return true;
    }
    //std::cout << "Exiting FIELDSEP." << std::endl;
    return false;
}

//const char* test_str = "if (t >= -5) and (#z ~= 51515) then a = b.some_function(\"this is a string\", [[this is a string \"with quotes\" in it.]]) elseif t < 5 then f2 = f.another_function(52, 33) end";
const char* test_str = "if a > 5 then b = 101 else b = 52 end";
//const char* test_str = "a = 5";

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
    std::cout << token_to_str( lex_next(test_str, &pos, len, &test ) ) << std::endl;
    getsym(test_str, &pos, len);
    
    //std::cout << "First symbol: " << token_to_str( t ) << std::endl;
    
    // go
    std::cout << "Running parser." << std::endl;
    if( chunk(test_str, &pos, len) ) {
        std::cout << "Parser seemed to accept it without problems." << std::endl;
    } else {
        std::cout << "Parser encountered a syntax error." << std::endl;
    }
    return 0;
}