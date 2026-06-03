export module parser.grammar;

import std;
import std.meta;
import lexer.token;
export import parser.symbol;

export enum production_rule : u8 {
    accept = 0;
    program = 1;
    functions_append = 2;
    functions_empty = 3;
    function = 4;
    return_int = 5;
    return_void = 6;
    params_some = 7;
    params_empty = 8;
    params_append = 9;
    params_one = 10;
    param_scalar = 11;
    block = 12;
    stmts_prepend = 13;
    stmts_empty = 14;
    stmt_var = 15;
    stmt_identifier = 16;
    stmt_if = 17;
    stmt_while = 18;
    stmt_return = 19;
    stmt_block = 20;
    var_decl = 21;
    decls_append = 22;
    decls_one = 23;
    decl_name = 24;
    decl_init = 25;
    assign = 26;
    call_stmt = 27;
    if_stmt = 28;
    else_block = 29;
    else_empty = 30;
    while_stmt = 31;
    return_stmt = 32;
    return_value = 33;
    return_empty = 34;
    expr_or = 35;
    or_binary = 36;
    or_and = 37;
    and_binary = 38;
    and_equality = 39;
    equal_binary = 40;
    not_equal_binary = 41;
    equality_rel = 42;
    less_binary = 43;
    less_equal_binary = 44;
    greater_binary = 45;
    greater_equal_binary = 46;
    rel_add = 47;
    add_binary = 48;
    sub_binary = 49;
    add_mul = 50;
    mul_binary = 51;
    div_binary = 52;
    mod_binary = 53;
    mul_unary = 54;
    unary_plus = 55;
    unary_minus = 56;
    unary_primary = 57;
    integer = 58;
    name = 59;
    call_expr = 60;
    grouped = 61;
    args_some = 62;
    args_empty = 63;
    args_append = 64;
    args_one = 65;
    param_array = 66;
    stmt_for = 67;
    stmt_break = 68;
    stmt_continue = 69;
    decl_array = 70;
    decl_array_init = 71;
    assign_index = 72;
    index_expr = 73;
    for_stmt = 74;
    for_init_decl = 75;
    for_init_assign = 76;
    for_init_index = 77;
    for_init_empty = 78;
    for_step_assign = 79;
    for_step_index = 80;
    for_step_empty = 81;
    break_stmt = 82;
    continue_stmt = 83;
    init_list = 84;
    init_values_some = 85;
    init_values_empty = 86;
    init_values_append = 87;
    init_values_one = 88;
}

export struct production {
    rule: production_rule = production_rule::accept;
    lhs: nonterminal_kind = nonterminal_kind::augmented;
    rhs: vector<grammar_symbol> = vector<grammar_symbol>{};
}

export struct grammar {
    productions: vector<production>;
}

grammar_symbol_of<T>(value: T const&) -> grammar_symbol
{
    template if(read_type<decltype(value)> == grammar_symbol) {
        return value;
    } else template if(read_type<decltype(value)> == token_kind) {
        return symbol_terminal(value);
    } else template if(read_type<decltype(value)> == nonterminal_kind) {
        return symbol_nonterminal(value);
    } else {
        return value;
    }
}

add_production<T...>(grammar: grammar&, rule: production_rule, lhs: nonterminal_kind, rhs: T...) -> void
{
    let symbols = vector<grammar_symbol>{};
    template for(const ref value : rhs...) {
        symbols.push_back(grammar_symbol_of(value));
    }
    grammar.productions.push_back(production{ .rule = rule, .lhs = lhs, .rhs = move symbols });
}

export make_minic_grammar() -> grammar
{
    let grammar = grammar{ .productions = vector<production>{} };

    // MiniC 的 LR(1) 文法直接写在最外层 API 里，main 只需要调用 build_parser_tables。
    // rule 是规约语义动作的稳定名字；产生式在 vector 中的下标只服务于表构造。
    add_production(grammar, production_rule::accept, nonterminal_kind::augmented, nonterminal_kind::program);
    add_production(grammar, production_rule::program, nonterminal_kind::program, nonterminal_kind::function_list);
    add_production(grammar, production_rule::functions_append, nonterminal_kind::function_list, nonterminal_kind::function_list, nonterminal_kind::function_item);
    add_production(grammar, production_rule::functions_empty, nonterminal_kind::function_list);
    add_production(grammar, production_rule::function, nonterminal_kind::function_item, nonterminal_kind::return_type, token_kind::identifier, token_kind::l_paren, nonterminal_kind::parameter_list_opt, token_kind::r_paren, nonterminal_kind::block);
    add_production(grammar, production_rule::return_int, nonterminal_kind::return_type, token_kind::kw_int);
    add_production(grammar, production_rule::return_void, nonterminal_kind::return_type, token_kind::kw_void);
    add_production(grammar, production_rule::params_some, nonterminal_kind::parameter_list_opt, nonterminal_kind::parameter_list);
    add_production(grammar, production_rule::params_empty, nonterminal_kind::parameter_list_opt);
    add_production(grammar, production_rule::params_append, nonterminal_kind::parameter_list, nonterminal_kind::parameter_list, token_kind::comma, nonterminal_kind::parameter_item);
    add_production(grammar, production_rule::params_one, nonterminal_kind::parameter_list, nonterminal_kind::parameter_item);
    add_production(grammar, production_rule::param_scalar, nonterminal_kind::parameter_item, token_kind::kw_int, token_kind::identifier);
    add_production(grammar, production_rule::block, nonterminal_kind::block, token_kind::l_brace, nonterminal_kind::statement_list, token_kind::r_brace);
    add_production(grammar, production_rule::stmts_prepend, nonterminal_kind::statement_list, nonterminal_kind::statement, nonterminal_kind::statement_list);
    add_production(grammar, production_rule::stmts_empty, nonterminal_kind::statement_list);
    add_production(grammar, production_rule::stmt_var, nonterminal_kind::statement, nonterminal_kind::var_decl);
    add_production(grammar, production_rule::stmt_identifier, nonterminal_kind::statement, nonterminal_kind::identifier_statement);
    add_production(grammar, production_rule::stmt_if, nonterminal_kind::statement, nonterminal_kind::if_statement_nt);
    add_production(grammar, production_rule::stmt_while, nonterminal_kind::statement, nonterminal_kind::while_statement_nt);
    add_production(grammar, production_rule::stmt_return, nonterminal_kind::statement, nonterminal_kind::return_statement_nt);
    add_production(grammar, production_rule::stmt_block, nonterminal_kind::statement, nonterminal_kind::block);
    add_production(grammar, production_rule::var_decl, nonterminal_kind::var_decl, token_kind::kw_int, nonterminal_kind::var_decl_list, token_kind::semicolon);
    add_production(grammar, production_rule::decls_append, nonterminal_kind::var_decl_list, nonterminal_kind::var_decl_list, token_kind::comma, nonterminal_kind::var_decl_item_nt);
    add_production(grammar, production_rule::decls_one, nonterminal_kind::var_decl_list, nonterminal_kind::var_decl_item_nt);
    add_production(grammar, production_rule::decl_name, nonterminal_kind::var_decl_item_nt, token_kind::identifier);
    add_production(grammar, production_rule::decl_init, nonterminal_kind::var_decl_item_nt, token_kind::identifier, token_kind::equal, nonterminal_kind::expression);
    add_production(grammar, production_rule::assign, nonterminal_kind::identifier_statement, token_kind::identifier, token_kind::equal, nonterminal_kind::expression, token_kind::semicolon);
    add_production(grammar, production_rule::call_stmt, nonterminal_kind::identifier_statement, token_kind::identifier, token_kind::l_paren, nonterminal_kind::argument_list_opt, token_kind::r_paren, token_kind::semicolon);
    add_production(grammar, production_rule::if_stmt, nonterminal_kind::if_statement_nt, token_kind::kw_if, token_kind::l_paren, nonterminal_kind::expression, token_kind::r_paren, nonterminal_kind::block, nonterminal_kind::else_opt);
    add_production(grammar, production_rule::else_block, nonterminal_kind::else_opt, token_kind::kw_else, nonterminal_kind::block);
    add_production(grammar, production_rule::else_empty, nonterminal_kind::else_opt);
    add_production(grammar, production_rule::while_stmt, nonterminal_kind::while_statement_nt, token_kind::kw_while, token_kind::l_paren, nonterminal_kind::expression, token_kind::r_paren, nonterminal_kind::block);
    add_production(grammar, production_rule::return_stmt, nonterminal_kind::return_statement_nt, token_kind::kw_return, nonterminal_kind::return_value_opt, token_kind::semicolon);
    add_production(grammar, production_rule::return_value, nonterminal_kind::return_value_opt, nonterminal_kind::expression);
    add_production(grammar, production_rule::return_empty, nonterminal_kind::return_value_opt);
    add_production(grammar, production_rule::expr_or, nonterminal_kind::expression, nonterminal_kind::logical_or);
    add_production(grammar, production_rule::or_binary, nonterminal_kind::logical_or, nonterminal_kind::logical_or, token_kind::pipe_pipe, nonterminal_kind::logical_and);
    add_production(grammar, production_rule::or_and, nonterminal_kind::logical_or, nonterminal_kind::logical_and);
    add_production(grammar, production_rule::and_binary, nonterminal_kind::logical_and, nonterminal_kind::logical_and, token_kind::amp_amp, nonterminal_kind::equality);
    add_production(grammar, production_rule::and_equality, nonterminal_kind::logical_and, nonterminal_kind::equality);
    add_production(grammar, production_rule::equal_binary, nonterminal_kind::equality, nonterminal_kind::equality, token_kind::equal_equal, nonterminal_kind::relational);
    add_production(grammar, production_rule::not_equal_binary, nonterminal_kind::equality, nonterminal_kind::equality, token_kind::bang_equal, nonterminal_kind::relational);
    add_production(grammar, production_rule::equality_rel, nonterminal_kind::equality, nonterminal_kind::relational);
    add_production(grammar, production_rule::less_binary, nonterminal_kind::relational, nonterminal_kind::relational, token_kind::less, nonterminal_kind::additive);
    add_production(grammar, production_rule::less_equal_binary, nonterminal_kind::relational, nonterminal_kind::relational, token_kind::less_equal, nonterminal_kind::additive);
    add_production(grammar, production_rule::greater_binary, nonterminal_kind::relational, nonterminal_kind::relational, token_kind::greater, nonterminal_kind::additive);
    add_production(grammar, production_rule::greater_equal_binary, nonterminal_kind::relational, nonterminal_kind::relational, token_kind::greater_equal, nonterminal_kind::additive);
    add_production(grammar, production_rule::rel_add, nonterminal_kind::relational, nonterminal_kind::additive);
    add_production(grammar, production_rule::add_binary, nonterminal_kind::additive, nonterminal_kind::additive, token_kind::plus, nonterminal_kind::multiplicative);
    add_production(grammar, production_rule::sub_binary, nonterminal_kind::additive, nonterminal_kind::additive, token_kind::minus, nonterminal_kind::multiplicative);
    add_production(grammar, production_rule::add_mul, nonterminal_kind::additive, nonterminal_kind::multiplicative);
    add_production(grammar, production_rule::mul_binary, nonterminal_kind::multiplicative, nonterminal_kind::multiplicative, token_kind::star, nonterminal_kind::unary);
    add_production(grammar, production_rule::div_binary, nonterminal_kind::multiplicative, nonterminal_kind::multiplicative, token_kind::slash, nonterminal_kind::unary);
    add_production(grammar, production_rule::mod_binary, nonterminal_kind::multiplicative, nonterminal_kind::multiplicative, token_kind::percent, nonterminal_kind::unary);
    add_production(grammar, production_rule::mul_unary, nonterminal_kind::multiplicative, nonterminal_kind::unary);
    add_production(grammar, production_rule::unary_plus, nonterminal_kind::unary, token_kind::plus, nonterminal_kind::unary);
    add_production(grammar, production_rule::unary_minus, nonterminal_kind::unary, token_kind::minus, nonterminal_kind::unary);
    add_production(grammar, production_rule::unary_primary, nonterminal_kind::unary, nonterminal_kind::primary);
    add_production(grammar, production_rule::integer, nonterminal_kind::primary, token_kind::integer_literal);
    add_production(grammar, production_rule::name, nonterminal_kind::primary, token_kind::identifier);
    add_production(grammar, production_rule::call_expr, nonterminal_kind::primary, token_kind::identifier, token_kind::l_paren, nonterminal_kind::argument_list_opt, token_kind::r_paren);
    add_production(grammar, production_rule::grouped, nonterminal_kind::primary, token_kind::l_paren, nonterminal_kind::expression, token_kind::r_paren);
    add_production(grammar, production_rule::args_some, nonterminal_kind::argument_list_opt, nonterminal_kind::argument_list);
    add_production(grammar, production_rule::args_empty, nonterminal_kind::argument_list_opt);
    add_production(grammar, production_rule::args_append, nonterminal_kind::argument_list, nonterminal_kind::argument_list, token_kind::comma, nonterminal_kind::expression);
    add_production(grammar, production_rule::args_one, nonterminal_kind::argument_list, nonterminal_kind::expression);
    add_production(grammar, production_rule::param_array, nonterminal_kind::parameter_item, token_kind::kw_int, token_kind::identifier, token_kind::l_bracket, token_kind::r_bracket);
    add_production(grammar, production_rule::stmt_for, nonterminal_kind::statement, nonterminal_kind::for_statement_nt);
    add_production(grammar, production_rule::stmt_break, nonterminal_kind::statement, nonterminal_kind::break_statement_nt);
    add_production(grammar, production_rule::stmt_continue, nonterminal_kind::statement, nonterminal_kind::continue_statement_nt);
    add_production(grammar, production_rule::decl_array, nonterminal_kind::var_decl_item_nt, token_kind::identifier, token_kind::l_bracket, token_kind::integer_literal, token_kind::r_bracket);
    add_production(grammar, production_rule::decl_array_init, nonterminal_kind::var_decl_item_nt, token_kind::identifier, token_kind::l_bracket, token_kind::integer_literal, token_kind::r_bracket, token_kind::equal, nonterminal_kind::initializer_list);
    add_production(grammar, production_rule::assign_index, nonterminal_kind::identifier_statement, token_kind::identifier, token_kind::l_bracket, nonterminal_kind::expression, token_kind::r_bracket, token_kind::equal, nonterminal_kind::expression, token_kind::semicolon);
    add_production(grammar, production_rule::index_expr, nonterminal_kind::primary, token_kind::identifier, token_kind::l_bracket, nonterminal_kind::expression, token_kind::r_bracket);
    add_production(grammar, production_rule::for_stmt, nonterminal_kind::for_statement_nt, token_kind::kw_for, token_kind::l_paren, nonterminal_kind::for_init_opt, token_kind::semicolon, nonterminal_kind::expression, token_kind::semicolon, nonterminal_kind::for_step_opt, token_kind::r_paren, nonterminal_kind::block);
    add_production(grammar, production_rule::for_init_decl, nonterminal_kind::for_init_opt, token_kind::kw_int, nonterminal_kind::var_decl_list);
    add_production(grammar, production_rule::for_init_assign, nonterminal_kind::for_init_opt, token_kind::identifier, token_kind::equal, nonterminal_kind::expression);
    add_production(grammar, production_rule::for_init_index, nonterminal_kind::for_init_opt, token_kind::identifier, token_kind::l_bracket, nonterminal_kind::expression, token_kind::r_bracket, token_kind::equal, nonterminal_kind::expression);
    add_production(grammar, production_rule::for_init_empty, nonterminal_kind::for_init_opt);
    add_production(grammar, production_rule::for_step_assign, nonterminal_kind::for_step_opt, token_kind::identifier, token_kind::equal, nonterminal_kind::expression);
    add_production(grammar, production_rule::for_step_index, nonterminal_kind::for_step_opt, token_kind::identifier, token_kind::l_bracket, nonterminal_kind::expression, token_kind::r_bracket, token_kind::equal, nonterminal_kind::expression);
    add_production(grammar, production_rule::for_step_empty, nonterminal_kind::for_step_opt);
    add_production(grammar, production_rule::break_stmt, nonterminal_kind::break_statement_nt, token_kind::kw_break, token_kind::semicolon);
    add_production(grammar, production_rule::continue_stmt, nonterminal_kind::continue_statement_nt, token_kind::kw_continue, token_kind::semicolon);
    add_production(grammar, production_rule::init_list, nonterminal_kind::initializer_list, token_kind::l_brace, nonterminal_kind::initializer_values_opt, token_kind::r_brace);
    add_production(grammar, production_rule::init_values_some, nonterminal_kind::initializer_values_opt, nonterminal_kind::initializer_values);
    add_production(grammar, production_rule::init_values_empty, nonterminal_kind::initializer_values_opt);
    add_production(grammar, production_rule::init_values_append, nonterminal_kind::initializer_values, nonterminal_kind::initializer_values, token_kind::comma, nonterminal_kind::expression);
    add_production(grammar, production_rule::init_values_one, nonterminal_kind::initializer_values, nonterminal_kind::expression);
    return grammar;
}
