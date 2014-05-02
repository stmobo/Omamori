// lua_ast.cpp
// AST (abstract syntax tree) classes for Lua

/*
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
*/

#define AST_CONSTANT_BOOL          0
#define AST_CONSTANT_INT           1
#define AST_CONSTANT_STRING        2
#define AST_CONSTANT_NIL           3

enum struct ast_datatype : char {
    boolean,
    integer,
    floating_point,
    string,
    table,
    nil,
    variable,
};

enum struct ast_stat_type : char {
    do_block,
    while_block,
    repeat_block,
    if_block,
    numeric_for,
    iterator_for,
    nonlocal_function,
    local_function,
    local_assignment,
    nonlocal_assignment,
    function_call,
};

enum struct ast_binop : char {
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
    oper_or,
    oper_and,
};

enum struct ast_unop : char {
    oper_not,
    oper_inverse,
    oper_len,
};

struct ast_node {
    ast_node_type node_type;
}

struct ast_statement : ast_node {
    ast_stat_type type,
    ast_statement* next, // pointer to next statement in block
};

struct ast_data : ast_node { // constants & variables
    ast_data_type type,
    union {
        bool boolean,
        int integer,
        double floating,
        const char* string,
    } value,
    ast_data* next,
};

struct ast_block : ast_node {
    ast_statement *first,
    ast_variable *locals,
};

// P = unop Exp(q) | '(' exp ')' | data_var
struct ast_exp_prefix : ast_node {
    ast_unop op,
    ast_exp exp,
    ast_data data,
};

struct ast_exp : ast_node {
    ast_exp_prefix *lhs,
    ast_exp *rhs,
    ast_binop op,
};

struct ast_func : ast_node {
    ast_parameter parameters,
    char* name,
    ast_block body,
};