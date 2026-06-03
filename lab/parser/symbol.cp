export module parser.symbol;

import std;
export import lexer.token;

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

export variant symbol_data {
    // 终结符 terminal：词法分析器已经切好的最小输入单元。
    // 例如 `int`、identifier、`(`、`;` 都是终结符；LR 分析器只能读入/匹配它们，不能再把它们展开成更小的语法结构。
    terminal(token_kind);
    // 非终结符 nonterminal：文法中人为定义的语法类别。
    // 例如 expression、statement、function_item；它们不是输入 token，必须通过产生式继续展开，最后才能落到终结符。
    nonterminal(nonterminal_kind);
    // epsilon：理论上的空串，只在 FIRST 集传播时当标记使用。
    // 真实产生式右部仍用空 vector 表示空产生式，输入流里永远不会出现 epsilon token。
    epsilon;
}

export struct grammar_symbol {
    data: symbol_data = symbol_data::epsilon;
}

impl grammar_symbol {
    grammar_symbol(data: symbol_data)
    {
        return grammar_symbol{ .data = data };
    }

    operator ==(self const&, rhs: this const&) -> bool
    {
        return match data {
            .terminal(left) => match rhs.data {
                .terminal(right) => left == right,
                _ => false,
            },
            .nonterminal(left) => match rhs.data {
                .nonterminal(right) => left == right,
                _ => false,
            },
            .epsilon => match rhs.data {
                .epsilon => true,
                _ => false,
            },
        };
    }

    operator <=>(self const&, rhs: this const&) -> weak_ordering
    {
        let left_rank = symbol_rank(self);
        let right_rank = symbol_rank(rhs);
        if(left_rank != right_rank) {
            return left_rank <=> right_rank;
        }
        return match data {
            .terminal(left) => match rhs.data {
                .terminal(right) => left <=> right,
                _ => weak_ordering::equivalent,
            },
            .nonterminal(left) => match rhs.data {
                .nonterminal(right) => left <=> right,
                _ => weak_ordering::equivalent,
            },
            .epsilon => weak_ordering::equivalent,
        };
    }
}

export symbol_terminal(kind: token_kind) -> grammar_symbol
{
    return grammar_symbol{symbol_data::terminal(kind)};
}

export symbol_nonterminal(kind: nonterminal_kind) -> grammar_symbol
{
    return grammar_symbol{symbol_data::nonterminal(kind)};
}

export nonterminal_kind_name(kind: nonterminal_kind) -> str
{
    if(kind == nonterminal_kind::augmented) { return "augmented"; }
    if(kind == nonterminal_kind::program) { return "program"; }
    if(kind == nonterminal_kind::function_list) { return "function_list"; }
    if(kind == nonterminal_kind::function_item) { return "function_item"; }
    if(kind == nonterminal_kind::return_type) { return "return_type"; }
    if(kind == nonterminal_kind::parameter_list_opt) { return "parameter_list_opt"; }
    if(kind == nonterminal_kind::parameter_list) { return "parameter_list"; }
    if(kind == nonterminal_kind::parameter_item) { return "parameter_item"; }
    if(kind == nonterminal_kind::block) { return "block"; }
    if(kind == nonterminal_kind::statement_list) { return "statement_list"; }
    if(kind == nonterminal_kind::statement) { return "statement"; }
    if(kind == nonterminal_kind::var_decl) { return "var_decl"; }
    if(kind == nonterminal_kind::var_decl_list) { return "var_decl_list"; }
    if(kind == nonterminal_kind::var_decl_item_nt) { return "var_decl_item"; }
    if(kind == nonterminal_kind::identifier_statement) { return "identifier_statement"; }
    if(kind == nonterminal_kind::if_statement_nt) { return "if_statement"; }
    if(kind == nonterminal_kind::else_opt) { return "else_opt"; }
    if(kind == nonterminal_kind::while_statement_nt) { return "while_statement"; }
    if(kind == nonterminal_kind::return_statement_nt) { return "return_statement"; }
    if(kind == nonterminal_kind::return_value_opt) { return "return_value_opt"; }
    if(kind == nonterminal_kind::expression) { return "expression"; }
    if(kind == nonterminal_kind::logical_or) { return "logical_or"; }
    if(kind == nonterminal_kind::logical_and) { return "logical_and"; }
    if(kind == nonterminal_kind::equality) { return "equality"; }
    if(kind == nonterminal_kind::relational) { return "relational"; }
    if(kind == nonterminal_kind::additive) { return "additive"; }
    if(kind == nonterminal_kind::multiplicative) { return "multiplicative"; }
    if(kind == nonterminal_kind::unary) { return "unary"; }
    if(kind == nonterminal_kind::primary) { return "primary"; }
    if(kind == nonterminal_kind::argument_list_opt) { return "argument_list_opt"; }
    if(kind == nonterminal_kind::argument_list) { return "argument_list"; }
    if(kind == nonterminal_kind::for_statement_nt) { return "for_statement"; }
    if(kind == nonterminal_kind::for_init_opt) { return "for_init_opt"; }
    if(kind == nonterminal_kind::for_step_opt) { return "for_step_opt"; }
    if(kind == nonterminal_kind::break_statement_nt) { return "break_statement"; }
    if(kind == nonterminal_kind::continue_statement_nt) { return "continue_statement"; }
    if(kind == nonterminal_kind::initializer_list) { return "initializer_list"; }
    if(kind == nonterminal_kind::initializer_values_opt) { return "initializer_values_opt"; }
    return "initializer_values";
}

export symbol_epsilon() -> grammar_symbol
{
    return grammar_symbol{symbol_data::epsilon};
}

export symbol_rank(symbol: grammar_symbol const&) -> u8
{
    return match symbol.data {
        .terminal(kind) => 0 as u8,
        .nonterminal(kind) => 1 as u8,
        .epsilon => 2 as u8,
    };
}

export symbol_is_terminal(symbol: grammar_symbol const&) -> bool
{
    return match symbol.data {
        .terminal(kind) => true,
        _ => false,
    };
}

export symbol_is_nonterminal(symbol: grammar_symbol const&) -> bool
{
    return match symbol.data {
        .nonterminal(kind) => true,
        _ => false,
    };
}

export symbol_is_epsilon(symbol: grammar_symbol const&) -> bool
{
    return match symbol.data {
        .epsilon => true,
        _ => false,
    };
}

export symbol_terminal_kind(symbol: grammar_symbol const&) -> token_kind
{
    return match symbol.data {
        .terminal(kind) => kind,
        _ => panic("grammar symbol is not terminal"),
    };
}

export symbol_nonterminal_kind(symbol: grammar_symbol const&) -> nonterminal_kind
{
    return match symbol.data {
        .nonterminal(kind) => kind,
        _ => panic("grammar symbol is not nonterminal"),
    };
}

export symbol_equal(left: grammar_symbol const&, right: grammar_symbol const&) -> bool
{
    return left == right;
}
