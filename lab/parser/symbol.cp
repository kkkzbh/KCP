export module parser.symbol;

import std;
export import lexer.token;

export enum grammar_symbol_kind : u8 {
    // 终结符 terminal：词法分析器已经切好的最小输入单元。
    // 例如 `int`、identifier、`(`、`;` 都是终结符；LR 分析器只能读入/匹配它们，不能再把它们展开成更小的语法结构。
    terminal = 0;
    // 非终结符 nonterminal：文法中人为定义的语法类别。
    // 例如 expression、statement、function_item；它们不是输入 token，必须通过产生式继续展开，最后才能落到终结符。
    nonterminal = 1;
    // epsilon：理论上的空串，只在 FIRST 集传播时当标记使用。
    // 真实产生式右部仍用空 vector 表示空产生式，输入流里永远不会出现 epsilon token。
    epsilon = 2;
}

export enum nonterminal_kind : u8 {
    augmented = 0;
    program = 1;
    function_list = 2;
    function_item = 3;
    return_type = 4;
    parameter_list_opt = 5;
    parameter_list = 6;
    parameter_item = 7;
    block = 8;
    statement_list = 9;
    statement = 10;
    var_decl = 11;
    var_decl_list = 12;
    var_decl_item_nt = 13;
    identifier_statement = 14;
    if_statement_nt = 15;
    else_opt = 16;
    while_statement_nt = 17;
    return_statement_nt = 18;
    return_value_opt = 19;
    expression = 20;
    logical_or = 21;
    logical_and = 22;
    equality = 23;
    relational = 24;
    additive = 25;
    multiplicative = 26;
    unary = 27;
    primary = 28;
    argument_list_opt = 29;
    argument_list = 30;
    for_statement_nt = 31;
    for_init_opt = 32;
    for_step_opt = 33;
    break_statement_nt = 34;
    continue_statement_nt = 35;
    initializer_list = 36;
    initializer_values_opt = 37;
    initializer_values = 38;
}

export struct grammar_symbol {
    kind: grammar_symbol_kind;
    terminal: token_kind;
    nonterminal: nonterminal_kind;
}

impl grammar_symbol {
    grammar_symbol()
    {
        return grammar_symbol{
            .kind = grammar_symbol_kind::epsilon,
            .terminal = token_kind::eof,
            .nonterminal = nonterminal_kind::augmented
        };
    }
}

export token_rank(kind: token_kind) -> i32
{
    if(kind == token_kind::eof) { return 0; }
    if(kind == token_kind::invalid) { return 1; }
    if(kind == token_kind::identifier) { return 2; }
    if(kind == token_kind::integer_literal) { return 3; }
    if(kind == token_kind::kw_int) { return 4; }
    if(kind == token_kind::kw_void) { return 5; }
    if(kind == token_kind::kw_if) { return 6; }
    if(kind == token_kind::kw_else) { return 7; }
    if(kind == token_kind::kw_while) { return 8; }
    if(kind == token_kind::kw_return) { return 9; }
    if(kind == token_kind::kw_for) { return 10; }
    if(kind == token_kind::kw_break) { return 11; }
    if(kind == token_kind::kw_continue) { return 12; }
    if(kind == token_kind::l_paren) { return 13; }
    if(kind == token_kind::r_paren) { return 14; }
    if(kind == token_kind::l_brace) { return 15; }
    if(kind == token_kind::r_brace) { return 16; }
    if(kind == token_kind::l_bracket) { return 17; }
    if(kind == token_kind::r_bracket) { return 18; }
    if(kind == token_kind::comma) { return 19; }
    if(kind == token_kind::semicolon) { return 20; }
    if(kind == token_kind::plus) { return 21; }
    if(kind == token_kind::minus) { return 22; }
    if(kind == token_kind::star) { return 23; }
    if(kind == token_kind::slash) { return 24; }
    if(kind == token_kind::percent) { return 25; }
    if(kind == token_kind::equal) { return 26; }
    if(kind == token_kind::equal_equal) { return 27; }
    if(kind == token_kind::bang_equal) { return 28; }
    if(kind == token_kind::less) { return 29; }
    if(kind == token_kind::less_equal) { return 30; }
    if(kind == token_kind::greater) { return 31; }
    if(kind == token_kind::greater_equal) { return 32; }
    if(kind == token_kind::amp_amp) { return 33; }
    if(kind == token_kind::pipe_pipe) { return 34; }
    return 35;
}

export nonterminal_rank(kind: nonterminal_kind) -> i32
{
    if(kind == nonterminal_kind::augmented) { return 0; }
    if(kind == nonterminal_kind::program) { return 1; }
    if(kind == nonterminal_kind::function_list) { return 2; }
    if(kind == nonterminal_kind::function_item) { return 3; }
    if(kind == nonterminal_kind::return_type) { return 4; }
    if(kind == nonterminal_kind::parameter_list_opt) { return 5; }
    if(kind == nonterminal_kind::parameter_list) { return 6; }
    if(kind == nonterminal_kind::parameter_item) { return 7; }
    if(kind == nonterminal_kind::block) { return 8; }
    if(kind == nonterminal_kind::statement_list) { return 9; }
    if(kind == nonterminal_kind::statement) { return 10; }
    if(kind == nonterminal_kind::var_decl) { return 11; }
    if(kind == nonterminal_kind::var_decl_list) { return 12; }
    if(kind == nonterminal_kind::var_decl_item_nt) { return 13; }
    if(kind == nonterminal_kind::identifier_statement) { return 14; }
    if(kind == nonterminal_kind::if_statement_nt) { return 15; }
    if(kind == nonterminal_kind::else_opt) { return 16; }
    if(kind == nonterminal_kind::while_statement_nt) { return 17; }
    if(kind == nonterminal_kind::return_statement_nt) { return 18; }
    if(kind == nonterminal_kind::return_value_opt) { return 19; }
    if(kind == nonterminal_kind::expression) { return 20; }
    if(kind == nonterminal_kind::logical_or) { return 21; }
    if(kind == nonterminal_kind::logical_and) { return 22; }
    if(kind == nonterminal_kind::equality) { return 23; }
    if(kind == nonterminal_kind::relational) { return 24; }
    if(kind == nonterminal_kind::additive) { return 25; }
    if(kind == nonterminal_kind::multiplicative) { return 26; }
    if(kind == nonterminal_kind::unary) { return 27; }
    if(kind == nonterminal_kind::primary) { return 28; }
    if(kind == nonterminal_kind::argument_list_opt) { return 29; }
    if(kind == nonterminal_kind::argument_list) { return 30; }
    if(kind == nonterminal_kind::for_statement_nt) { return 31; }
    if(kind == nonterminal_kind::for_init_opt) { return 32; }
    if(kind == nonterminal_kind::for_step_opt) { return 33; }
    if(kind == nonterminal_kind::break_statement_nt) { return 34; }
    if(kind == nonterminal_kind::continue_statement_nt) { return 35; }
    if(kind == nonterminal_kind::initializer_list) { return 36; }
    if(kind == nonterminal_kind::initializer_values_opt) { return 37; }
    return 38;
}

export symbol_kind_rank(kind: grammar_symbol_kind) -> i32
{
    if(kind == grammar_symbol_kind::terminal) { return 0; }
    if(kind == grammar_symbol_kind::nonterminal) { return 1; }
    return 2;
}

export symbol_terminal(kind: token_kind) -> grammar_symbol
{
    return grammar_symbol{
        .kind = grammar_symbol_kind::terminal,
        .terminal = kind,
        .nonterminal = nonterminal_kind::augmented
    };
}

export symbol_nonterminal(kind: nonterminal_kind) -> grammar_symbol
{
    return grammar_symbol{
        .kind = grammar_symbol_kind::nonterminal,
        .terminal = token_kind::eof,
        .nonterminal = kind
    };
}

export symbol_epsilon() -> grammar_symbol
{
    return grammar_symbol{
        .kind = grammar_symbol_kind::epsilon,
        .terminal = token_kind::eof,
        .nonterminal = nonterminal_kind::augmented
    };
}

export symbol_equal(left: grammar_symbol const&, right: grammar_symbol const&) -> bool
{
    return left.kind == right.kind and left.terminal == right.terminal and left.nonterminal == right.nonterminal;
}

export symbol_order(left: grammar_symbol const&, right: grammar_symbol const&) -> weak_ordering
{
    if(left.kind != right.kind) {
        return symbol_kind_rank(left.kind) <=> symbol_kind_rank(right.kind);
    }
    if(left.terminal != right.terminal) {
        return token_rank(left.terminal) <=> token_rank(right.terminal);
    }
    return nonterminal_rank(left.nonterminal) <=> nonterminal_rank(right.nonterminal);
}

export struct grammar_symbol_order {
}

impl grammar_symbol_order {
    operator ()(self const&, left: grammar_symbol const&, right: grammar_symbol const&) -> weak_ordering
    {
        return symbol_order(left, right);
    }
}
