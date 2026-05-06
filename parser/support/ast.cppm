export module parser.ast;

import std;
import source;
import lexer.token;

export [[nodiscard]] auto combine_spans(source_span left, source_span right) -> source_span;

export struct qualified_name_syntax
{
    source_span full_span{};
    std::vector<source_span> components{};
};

export struct type_syntax
{
    source_span full_span{};
    qualified_name_syntax name{};
    std::vector<std::unique_ptr<type_syntax>> type_arguments{};
    std::vector<source_span> literal_arguments{};
    bool const_qualified{};
    std::vector<token_kind> suffix_operators{};
};

export enum class expr_syntax_kind
{
    name,
    literal,
    unary,
    binary,
    assignment,
    call,
    cast,
    array_literal,
    sequence_literal,
    tuple_literal,
    grouped,
};

export struct expr_syntax
{
    source_span full_span{};
    expr_syntax_kind kind{};
    token_kind operator_kind{ token_kind::eof };
    qualified_name_syntax name{};
    std::vector<std::unique_ptr<expr_syntax>> operands{};
    std::unique_ptr<type_syntax> type_operand{};
};

export enum class statement_syntax_kind
{
    block,
    declaration,
    if_stmt,
    while_stmt,
    do_while_stmt,
    for_stmt,
    break_stmt,
    continue_stmt,
    return_stmt,
    expr_stmt,
};

export struct statement_syntax
{
    source_span full_span{};
    statement_syntax_kind kind{};
    bool is_const{};
    std::optional<source_span> name{};
    std::optional<source_span> label{};
    std::unique_ptr<type_syntax> declared_type{};
    std::vector<std::unique_ptr<expr_syntax>> expressions{};
    std::vector<std::unique_ptr<statement_syntax>> statements{};
};

export struct parameter_syntax
{
    source_span full_span{};
    bool is_const{};
    source_span name{};
    std::unique_ptr<type_syntax> type{};
};

export struct function_syntax
{
    source_span full_span{};
    bool exported{};
    source_span name{};
    std::vector<parameter_syntax> parameters{};
    std::unique_ptr<type_syntax> return_type{};
    std::unique_ptr<statement_syntax> body{};
};

export struct import_syntax
{
    source_span full_span{};
    qualified_name_syntax name{};
};

export struct module_header_syntax
{
    source_span full_span{};
    bool exported{};
    qualified_name_syntax name{};
};

export struct translation_unit_syntax
{
    source_span full_span{};
    std::unique_ptr<module_header_syntax> module_header{};
    std::vector<import_syntax> imports{};
    std::vector<function_syntax> functions{};
};

auto combine_spans(source_span left, source_span right) -> source_span
{
    return source_span{
        .start = std::min(left.start, right.start),
        .end = std::max(left.end, right.end),
    };
}
