export module parser.grammar;

import std;
import lexer.token;
export import parser.symbol;

export struct production {
    lhs: nonterminal_kind;
    rhs: vector<grammar_symbol>;
}

impl production {
    production()
    {
        return production{
            .lhs = nonterminal_kind::augmented,
            .rhs = vector<grammar_symbol>{}
        };
    }
}

export struct grammar {
    productions: vector<production>;
}

symbols0() -> vector<grammar_symbol>
{
    return vector<grammar_symbol>{};
}

symbols1(first: grammar_symbol) -> vector<grammar_symbol>
{
    let result = vector<grammar_symbol>{};
    result.push_back(first);
    return result;
}

symbols2(first: grammar_symbol, second: grammar_symbol) -> vector<grammar_symbol>
{
    let result = vector<grammar_symbol>{};
    result.push_back(first);
    result.push_back(second);
    return result;
}

symbols3(first: grammar_symbol, second: grammar_symbol, third: grammar_symbol) -> vector<grammar_symbol>
{
    let result = vector<grammar_symbol>{};
    result.push_back(first);
    result.push_back(second);
    result.push_back(third);
    return result;
}

symbols4(first: grammar_symbol, second: grammar_symbol, third: grammar_symbol, fourth: grammar_symbol) -> vector<grammar_symbol>
{
    let result = vector<grammar_symbol>{};
    result.push_back(first);
    result.push_back(second);
    result.push_back(third);
    result.push_back(fourth);
    return result;
}

symbols5(first: grammar_symbol, second: grammar_symbol, third: grammar_symbol, fourth: grammar_symbol, fifth: grammar_symbol) -> vector<grammar_symbol>
{
    let result = vector<grammar_symbol>{};
    result.push_back(first);
    result.push_back(second);
    result.push_back(third);
    result.push_back(fourth);
    result.push_back(fifth);
    return result;
}

symbols6(first: grammar_symbol, second: grammar_symbol, third: grammar_symbol, fourth: grammar_symbol, fifth: grammar_symbol, sixth: grammar_symbol) -> vector<grammar_symbol>
{
    let result = vector<grammar_symbol>{};
    result.push_back(first);
    result.push_back(second);
    result.push_back(third);
    result.push_back(fourth);
    result.push_back(fifth);
    result.push_back(sixth);
    return result;
}

symbols7(first: grammar_symbol, second: grammar_symbol, third: grammar_symbol, fourth: grammar_symbol, fifth: grammar_symbol, sixth: grammar_symbol, seventh: grammar_symbol) -> vector<grammar_symbol>
{
    let result = vector<grammar_symbol>{};
    result.push_back(first);
    result.push_back(second);
    result.push_back(third);
    result.push_back(fourth);
    result.push_back(fifth);
    result.push_back(sixth);
    result.push_back(seventh);
    return result;
}

symbols9(first: grammar_symbol, second: grammar_symbol, third: grammar_symbol, fourth: grammar_symbol, fifth: grammar_symbol, sixth: grammar_symbol, seventh: grammar_symbol, eighth: grammar_symbol, ninth: grammar_symbol) -> vector<grammar_symbol>
{
    let result = vector<grammar_symbol>{};
    result.push_back(first);
    result.push_back(second);
    result.push_back(third);
    result.push_back(fourth);
    result.push_back(fifth);
    result.push_back(sixth);
    result.push_back(seventh);
    result.push_back(eighth);
    result.push_back(ninth);
    return result;
}

add_production(grammar: grammar&, lhs: nonterminal_kind, rhs: vector<grammar_symbol>) -> void
{
    grammar.productions.push_back(production{ .lhs = lhs, .rhs = move rhs });
}

export make_minic_grammar() -> grammar
{
    let grammar = grammar{ .productions = vector<production>{} };

    // MiniC 的 LR(1) 文法直接写在最外层 API 里，main 只需要调用 build_parser_tables。
    // 每一行 add_production 都对应一条产生式：左边 lhs 是一个非终结符，右边 rhs 是一串终结符/非终结符。
    // 这里的 0 号产生式是增广产生式 augmented -> program。LR 表里看到它规约完成并且向前看符号是 eof 时，就说明整个输入被接受。
    add_production(grammar, nonterminal_kind::augmented, symbols1(symbol_nonterminal(nonterminal_kind::program)));
    add_production(grammar, nonterminal_kind::program, symbols1(symbol_nonterminal(nonterminal_kind::function_list)));
    add_production(grammar, nonterminal_kind::function_list, symbols2(symbol_nonterminal(nonterminal_kind::function_list), symbol_nonterminal(nonterminal_kind::function_item)));
    add_production(grammar, nonterminal_kind::function_list, symbols0());
    add_production(grammar, nonterminal_kind::function_item, symbols6(symbol_nonterminal(nonterminal_kind::return_type), symbol_terminal(token_kind::identifier), symbol_terminal(token_kind::l_paren), symbol_nonterminal(nonterminal_kind::parameter_list_opt), symbol_terminal(token_kind::r_paren), symbol_nonterminal(nonterminal_kind::block)));
    add_production(grammar, nonterminal_kind::return_type, symbols1(symbol_terminal(token_kind::kw_int)));
    add_production(grammar, nonterminal_kind::return_type, symbols1(symbol_terminal(token_kind::kw_void)));
    add_production(grammar, nonterminal_kind::parameter_list_opt, symbols1(symbol_nonterminal(nonterminal_kind::parameter_list)));
    add_production(grammar, nonterminal_kind::parameter_list_opt, symbols0());
    add_production(grammar, nonterminal_kind::parameter_list, symbols3(symbol_nonterminal(nonterminal_kind::parameter_list), symbol_terminal(token_kind::comma), symbol_nonterminal(nonterminal_kind::parameter_item)));
    add_production(grammar, nonterminal_kind::parameter_list, symbols1(symbol_nonterminal(nonterminal_kind::parameter_item)));
    add_production(grammar, nonterminal_kind::parameter_item, symbols2(symbol_terminal(token_kind::kw_int), symbol_terminal(token_kind::identifier)));
    add_production(grammar, nonterminal_kind::block, symbols3(symbol_terminal(token_kind::l_brace), symbol_nonterminal(nonterminal_kind::statement_list), symbol_terminal(token_kind::r_brace)));
    add_production(grammar, nonterminal_kind::statement_list, symbols2(symbol_nonterminal(nonterminal_kind::statement), symbol_nonterminal(nonterminal_kind::statement_list)));
    add_production(grammar, nonterminal_kind::statement_list, symbols0());
    add_production(grammar, nonterminal_kind::statement, symbols1(symbol_nonterminal(nonterminal_kind::var_decl)));
    add_production(grammar, nonterminal_kind::statement, symbols1(symbol_nonterminal(nonterminal_kind::identifier_statement)));
    add_production(grammar, nonterminal_kind::statement, symbols1(symbol_nonterminal(nonterminal_kind::if_statement_nt)));
    add_production(grammar, nonterminal_kind::statement, symbols1(symbol_nonterminal(nonterminal_kind::while_statement_nt)));
    add_production(grammar, nonterminal_kind::statement, symbols1(symbol_nonterminal(nonterminal_kind::return_statement_nt)));
    add_production(grammar, nonterminal_kind::statement, symbols1(symbol_nonterminal(nonterminal_kind::block)));
    add_production(grammar, nonterminal_kind::var_decl, symbols3(symbol_terminal(token_kind::kw_int), symbol_nonterminal(nonterminal_kind::var_decl_list), symbol_terminal(token_kind::semicolon)));
    add_production(grammar, nonterminal_kind::var_decl_list, symbols3(symbol_nonterminal(nonterminal_kind::var_decl_list), symbol_terminal(token_kind::comma), symbol_nonterminal(nonterminal_kind::var_decl_item_nt)));
    add_production(grammar, nonterminal_kind::var_decl_list, symbols1(symbol_nonterminal(nonterminal_kind::var_decl_item_nt)));
    add_production(grammar, nonterminal_kind::var_decl_item_nt, symbols1(symbol_terminal(token_kind::identifier)));
    add_production(grammar, nonterminal_kind::var_decl_item_nt, symbols3(symbol_terminal(token_kind::identifier), symbol_terminal(token_kind::equal), symbol_nonterminal(nonterminal_kind::expression)));
    add_production(grammar, nonterminal_kind::identifier_statement, symbols4(symbol_terminal(token_kind::identifier), symbol_terminal(token_kind::equal), symbol_nonterminal(nonterminal_kind::expression), symbol_terminal(token_kind::semicolon)));
    add_production(grammar, nonterminal_kind::identifier_statement, symbols5(symbol_terminal(token_kind::identifier), symbol_terminal(token_kind::l_paren), symbol_nonterminal(nonterminal_kind::argument_list_opt), symbol_terminal(token_kind::r_paren), symbol_terminal(token_kind::semicolon)));
    add_production(grammar, nonterminal_kind::if_statement_nt, symbols6(symbol_terminal(token_kind::kw_if), symbol_terminal(token_kind::l_paren), symbol_nonterminal(nonterminal_kind::expression), symbol_terminal(token_kind::r_paren), symbol_nonterminal(nonterminal_kind::block), symbol_nonterminal(nonterminal_kind::else_opt)));
    add_production(grammar, nonterminal_kind::else_opt, symbols2(symbol_terminal(token_kind::kw_else), symbol_nonterminal(nonterminal_kind::block)));
    add_production(grammar, nonterminal_kind::else_opt, symbols0());
    add_production(grammar, nonterminal_kind::while_statement_nt, symbols5(symbol_terminal(token_kind::kw_while), symbol_terminal(token_kind::l_paren), symbol_nonterminal(nonterminal_kind::expression), symbol_terminal(token_kind::r_paren), symbol_nonterminal(nonterminal_kind::block)));
    add_production(grammar, nonterminal_kind::return_statement_nt, symbols3(symbol_terminal(token_kind::kw_return), symbol_nonterminal(nonterminal_kind::return_value_opt), symbol_terminal(token_kind::semicolon)));
    add_production(grammar, nonterminal_kind::return_value_opt, symbols1(symbol_nonterminal(nonterminal_kind::expression)));
    add_production(grammar, nonterminal_kind::return_value_opt, symbols0());
    add_production(grammar, nonterminal_kind::expression, symbols1(symbol_nonterminal(nonterminal_kind::logical_or)));
    add_production(grammar, nonterminal_kind::logical_or, symbols3(symbol_nonterminal(nonterminal_kind::logical_or), symbol_terminal(token_kind::pipe_pipe), symbol_nonterminal(nonterminal_kind::logical_and)));
    add_production(grammar, nonterminal_kind::logical_or, symbols1(symbol_nonterminal(nonterminal_kind::logical_and)));
    add_production(grammar, nonterminal_kind::logical_and, symbols3(symbol_nonterminal(nonterminal_kind::logical_and), symbol_terminal(token_kind::amp_amp), symbol_nonterminal(nonterminal_kind::equality)));
    add_production(grammar, nonterminal_kind::logical_and, symbols1(symbol_nonterminal(nonterminal_kind::equality)));
    add_production(grammar, nonterminal_kind::equality, symbols3(symbol_nonterminal(nonterminal_kind::equality), symbol_terminal(token_kind::equal_equal), symbol_nonterminal(nonterminal_kind::relational)));
    add_production(grammar, nonterminal_kind::equality, symbols3(symbol_nonterminal(nonterminal_kind::equality), symbol_terminal(token_kind::bang_equal), symbol_nonterminal(nonterminal_kind::relational)));
    add_production(grammar, nonterminal_kind::equality, symbols1(symbol_nonterminal(nonterminal_kind::relational)));
    add_production(grammar, nonterminal_kind::relational, symbols3(symbol_nonterminal(nonterminal_kind::relational), symbol_terminal(token_kind::less), symbol_nonterminal(nonterminal_kind::additive)));
    add_production(grammar, nonterminal_kind::relational, symbols3(symbol_nonterminal(nonterminal_kind::relational), symbol_terminal(token_kind::less_equal), symbol_nonterminal(nonterminal_kind::additive)));
    add_production(grammar, nonterminal_kind::relational, symbols3(symbol_nonterminal(nonterminal_kind::relational), symbol_terminal(token_kind::greater), symbol_nonterminal(nonterminal_kind::additive)));
    add_production(grammar, nonterminal_kind::relational, symbols3(symbol_nonterminal(nonterminal_kind::relational), symbol_terminal(token_kind::greater_equal), symbol_nonterminal(nonterminal_kind::additive)));
    add_production(grammar, nonterminal_kind::relational, symbols1(symbol_nonterminal(nonterminal_kind::additive)));
    add_production(grammar, nonterminal_kind::additive, symbols3(symbol_nonterminal(nonterminal_kind::additive), symbol_terminal(token_kind::plus), symbol_nonterminal(nonterminal_kind::multiplicative)));
    add_production(grammar, nonterminal_kind::additive, symbols3(symbol_nonterminal(nonterminal_kind::additive), symbol_terminal(token_kind::minus), symbol_nonterminal(nonterminal_kind::multiplicative)));
    add_production(grammar, nonterminal_kind::additive, symbols1(symbol_nonterminal(nonterminal_kind::multiplicative)));
    add_production(grammar, nonterminal_kind::multiplicative, symbols3(symbol_nonterminal(nonterminal_kind::multiplicative), symbol_terminal(token_kind::star), symbol_nonterminal(nonterminal_kind::unary)));
    add_production(grammar, nonterminal_kind::multiplicative, symbols3(symbol_nonterminal(nonterminal_kind::multiplicative), symbol_terminal(token_kind::slash), symbol_nonterminal(nonterminal_kind::unary)));
    add_production(grammar, nonterminal_kind::multiplicative, symbols3(symbol_nonterminal(nonterminal_kind::multiplicative), symbol_terminal(token_kind::percent), symbol_nonterminal(nonterminal_kind::unary)));
    add_production(grammar, nonterminal_kind::multiplicative, symbols1(symbol_nonterminal(nonterminal_kind::unary)));
    add_production(grammar, nonterminal_kind::unary, symbols2(symbol_terminal(token_kind::plus), symbol_nonterminal(nonterminal_kind::unary)));
    add_production(grammar, nonterminal_kind::unary, symbols2(symbol_terminal(token_kind::minus), symbol_nonterminal(nonterminal_kind::unary)));
    add_production(grammar, nonterminal_kind::unary, symbols1(symbol_nonterminal(nonterminal_kind::primary)));
    add_production(grammar, nonterminal_kind::primary, symbols1(symbol_terminal(token_kind::integer_literal)));
    add_production(grammar, nonterminal_kind::primary, symbols1(symbol_terminal(token_kind::identifier)));
    add_production(grammar, nonterminal_kind::primary, symbols4(symbol_terminal(token_kind::identifier), symbol_terminal(token_kind::l_paren), symbol_nonterminal(nonterminal_kind::argument_list_opt), symbol_terminal(token_kind::r_paren)));
    add_production(grammar, nonterminal_kind::primary, symbols3(symbol_terminal(token_kind::l_paren), symbol_nonterminal(nonterminal_kind::expression), symbol_terminal(token_kind::r_paren)));
    add_production(grammar, nonterminal_kind::argument_list_opt, symbols1(symbol_nonterminal(nonterminal_kind::argument_list)));
    add_production(grammar, nonterminal_kind::argument_list_opt, symbols0());
    add_production(grammar, nonterminal_kind::argument_list, symbols3(symbol_nonterminal(nonterminal_kind::argument_list), symbol_terminal(token_kind::comma), symbol_nonterminal(nonterminal_kind::expression)));
    add_production(grammar, nonterminal_kind::argument_list, symbols1(symbol_nonterminal(nonterminal_kind::expression)));
    add_production(grammar, nonterminal_kind::parameter_item, symbols4(symbol_terminal(token_kind::kw_int), symbol_terminal(token_kind::identifier), symbol_terminal(token_kind::l_bracket), symbol_terminal(token_kind::r_bracket)));
    add_production(grammar, nonterminal_kind::statement, symbols1(symbol_nonterminal(nonterminal_kind::for_statement_nt)));
    add_production(grammar, nonterminal_kind::statement, symbols1(symbol_nonterminal(nonterminal_kind::break_statement_nt)));
    add_production(grammar, nonterminal_kind::statement, symbols1(symbol_nonterminal(nonterminal_kind::continue_statement_nt)));
    add_production(grammar, nonterminal_kind::var_decl_item_nt, symbols4(symbol_terminal(token_kind::identifier), symbol_terminal(token_kind::l_bracket), symbol_terminal(token_kind::integer_literal), symbol_terminal(token_kind::r_bracket)));
    add_production(grammar, nonterminal_kind::var_decl_item_nt, symbols6(symbol_terminal(token_kind::identifier), symbol_terminal(token_kind::l_bracket), symbol_terminal(token_kind::integer_literal), symbol_terminal(token_kind::r_bracket), symbol_terminal(token_kind::equal), symbol_nonterminal(nonterminal_kind::initializer_list)));
    add_production(grammar, nonterminal_kind::identifier_statement, symbols7(symbol_terminal(token_kind::identifier), symbol_terminal(token_kind::l_bracket), symbol_nonterminal(nonterminal_kind::expression), symbol_terminal(token_kind::r_bracket), symbol_terminal(token_kind::equal), symbol_nonterminal(nonterminal_kind::expression), symbol_terminal(token_kind::semicolon)));
    add_production(grammar, nonterminal_kind::primary, symbols4(symbol_terminal(token_kind::identifier), symbol_terminal(token_kind::l_bracket), symbol_nonterminal(nonterminal_kind::expression), symbol_terminal(token_kind::r_bracket)));
    add_production(grammar, nonterminal_kind::for_statement_nt, symbols9(symbol_terminal(token_kind::kw_for), symbol_terminal(token_kind::l_paren), symbol_nonterminal(nonterminal_kind::for_init_opt), symbol_terminal(token_kind::semicolon), symbol_nonterminal(nonterminal_kind::expression), symbol_terminal(token_kind::semicolon), symbol_nonterminal(nonterminal_kind::for_step_opt), symbol_terminal(token_kind::r_paren), symbol_nonterminal(nonterminal_kind::block)));
    add_production(grammar, nonterminal_kind::for_init_opt, symbols2(symbol_terminal(token_kind::kw_int), symbol_nonterminal(nonterminal_kind::var_decl_list)));
    add_production(grammar, nonterminal_kind::for_init_opt, symbols3(symbol_terminal(token_kind::identifier), symbol_terminal(token_kind::equal), symbol_nonterminal(nonterminal_kind::expression)));
    add_production(grammar, nonterminal_kind::for_init_opt, symbols6(symbol_terminal(token_kind::identifier), symbol_terminal(token_kind::l_bracket), symbol_nonterminal(nonterminal_kind::expression), symbol_terminal(token_kind::r_bracket), symbol_terminal(token_kind::equal), symbol_nonterminal(nonterminal_kind::expression)));
    add_production(grammar, nonterminal_kind::for_init_opt, symbols0());
    add_production(grammar, nonterminal_kind::for_step_opt, symbols3(symbol_terminal(token_kind::identifier), symbol_terminal(token_kind::equal), symbol_nonterminal(nonterminal_kind::expression)));
    add_production(grammar, nonterminal_kind::for_step_opt, symbols6(symbol_terminal(token_kind::identifier), symbol_terminal(token_kind::l_bracket), symbol_nonterminal(nonterminal_kind::expression), symbol_terminal(token_kind::r_bracket), symbol_terminal(token_kind::equal), symbol_nonterminal(nonterminal_kind::expression)));
    add_production(grammar, nonterminal_kind::for_step_opt, symbols0());
    add_production(grammar, nonterminal_kind::break_statement_nt, symbols2(symbol_terminal(token_kind::kw_break), symbol_terminal(token_kind::semicolon)));
    add_production(grammar, nonterminal_kind::continue_statement_nt, symbols2(symbol_terminal(token_kind::kw_continue), symbol_terminal(token_kind::semicolon)));
    add_production(grammar, nonterminal_kind::initializer_list, symbols3(symbol_terminal(token_kind::l_brace), symbol_nonterminal(nonterminal_kind::initializer_values_opt), symbol_terminal(token_kind::r_brace)));
    add_production(grammar, nonterminal_kind::initializer_values_opt, symbols1(symbol_nonterminal(nonterminal_kind::initializer_values)));
    add_production(grammar, nonterminal_kind::initializer_values_opt, symbols0());
    add_production(grammar, nonterminal_kind::initializer_values, symbols3(symbol_nonterminal(nonterminal_kind::initializer_values), symbol_terminal(token_kind::comma), symbol_nonterminal(nonterminal_kind::expression)));
    add_production(grammar, nonterminal_kind::initializer_values, symbols1(symbol_nonterminal(nonterminal_kind::expression)));
    return grammar;
}
