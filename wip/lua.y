/* lua.y */

%token <token> cmp_less_or_equal cmp_greater_or_equal, cmp_equal, cmp_unequal, cmp_less cmp_greater
%token <token> oper_assignment oper_plus oper_minus oper_multiply oper_divide oper_exp oper_modulus
%token <token> comma open_paren close_paren open_curlybrace close_curlybrace open_squarebracket close_squarebracket period
%token <token> whitespace
%token <string> string identifier numeric_literal

%left oper_plus oper_minus
%left oper_multiply oper_divide

fieldlist : field fieldsep_list [fieldsep]

fieldsep_list : fieldsep field fieldsep_list | fieldsep field

field : open_squarebracket exp close_squarebracket oper_assignment exp | exp oper_assignment exp | exp

fieldsep : comma | semicolon

binop : oper_plus | oper_minus | oper_multiply | oper_divide | oper_exp | oper_modulus | cmp_less | cmp_greater | cmp_less_or_equal | cmp_greater_or_equal | cmp_equal | cmp_unequal | oper_and | oper_or

unop : oper_inverse | oper_not | oper_len