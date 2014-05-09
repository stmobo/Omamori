// lua_ast.h
// AST (abstract syntax tree) structures for Lua, in C++.

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

enum struct ast_data_type : char {
    boolean,
    number,
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
    last_break,
    last_return,
};

enum struct ast_node_type : char {
    data,
    exp,
    exp_prefix,
    statement,
    block,
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
};

struct ast_data : ast_node { // constants & variables
    ast_data_type type;
    union ast_data_value{
        bool boolean;
        double floating;
        const char* string;
    } value;
    ast_data* next;
    ast_data() {
        this->node_type = ast_node_type::data;
    };
};

// when type == ast_data_type::boolean || type == ast_data_type::number || type == ast_data_type::string, then the struct
// is talking about a constant -- you can find its value in this.value, under the corresponding name.
// when type == ast_data_type::variable, then the struct is talking about an identifier -- you can find its NAME
// under this.value.string.

struct ast_exp_prefix;
struct ast_exp;

// P = unop Exp(q) | '(' exp ')' | data_var
struct ast_exp_prefix : ast_node {
    ast_unop op;
    ast_exp *exp;
    ast_data *data;
    ast_exp_prefix() {
        this->node_type = ast_node_type::exp_prefix;
    };
};

struct ast_exp : ast_node {
    ast_exp_prefix *lhs;
    ast_exp *rhs;
    ast_binop op;
    ast_exp *next;
    ast_exp() {
        this->node_type = ast_node_type::exp;
    };
};

struct ast_statement : ast_node {
    ast_stat_type type;
    ast_statement* next; // pointer to next statement in block
    ast_statement() {
        this->node_type = ast_node_type::statement;
    };
};

struct ast_block : ast_node {
    ast_statement *first;
    ast_block() {
        this->node_type = ast_node_type::block;
    };
};

struct ast_return: ast_statement {
    ast_exp *ret;
    ast_return() {
        this->type = ast_stat_type::last_return;
    };
};

struct ast_do : ast_statement {
    ast_block *block;
    ast_do() {
        this->type = ast_stat_type::do_block;
    };
};

struct ast_func_call : ast_statement {
    char* func_name;
    ast_exp* parameters;
    ast_func_call() {
        this->type = ast_stat_type::function_call;
    };
};

struct ast_assignment : ast_statement {
    ast_data *lhs;
    ast_exp *rhs;
    ast_assignment* next; // assignment chaining
};

struct ast_loop : ast_statement { // while/repeat-until blocks - repetition type determined by this.type
    ast_exp *condition;
    ast_block *block;
};

struct ast_numeric_for : ast_statement {
    ast_exp *start;
    ast_exp *end;
    ast_exp *step = NULL;
    ast_block *block;
    ast_numeric_for() {
        this->type = ast_stat_type::numeric_for;
    };
};

struct ast_iterator_for : ast_statement {
    ast_data *namelist;
    ast_exp *iterator;
    ast_block *block;
    ast_iterator_for() {
        this->type = ast_stat_type::iterator_for;
    };
};

struct ast_branch : ast_statement { // while/repeat-until blocks - repetition type determined by this.type
    ast_exp *condition;
    ast_block *then_block;
    ast_branch* elseif = NULL;
    ast_block *else_block = NULL;
    ast_branch() {
        this->type = ast_stat_type::if_block;
    };
};

struct ast_func : ast_statement {
    ast_data* parameters;
    char *name;
    ast_block* body;
};