export module parser.ast;

import std;
import lexer.source;
import lexer.token;

export [[nodiscard]] auto combine_spans(span left, span right) -> span;

export struct qualified_name_syntax
{
    span full_span{};
    std::vector<span> components{};
};

export struct type_syntax
{
    span full_span{};
    qualified_name_syntax name{};
    std::vector<std::unique_ptr<type_syntax>> type_arguments{};
    std::vector<span> literal_arguments{};
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
    span full_span{};
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
    span full_span{};
    statement_syntax_kind kind{};
    bool is_const{};
    std::optional<span> name{};
    std::optional<span> label{};
    std::unique_ptr<type_syntax> declared_type{};
    std::vector<std::unique_ptr<expr_syntax>> expressions{};
    std::vector<std::unique_ptr<statement_syntax>> statements{};
};

export struct parameter_syntax
{
    span full_span{};
    bool is_const{};
    span name{};
    std::unique_ptr<type_syntax> type{};
};

export struct function_syntax
{
    span full_span{};
    bool exported{};
    span name{};
    std::vector<parameter_syntax> parameters{};
    std::unique_ptr<type_syntax> return_type{};
    std::unique_ptr<statement_syntax> body{};
};

export struct import_syntax
{
    span full_span{};
    qualified_name_syntax name{};
};

export struct module_header_syntax
{
    span full_span{};
    bool exported{};
    qualified_name_syntax name{};
};

export struct translation_unit_syntax
{
    span full_span{};
    std::unique_ptr<module_header_syntax> module_header{};
    std::vector<import_syntax> imports{};
    std::vector<function_syntax> functions{};
};

auto combine_spans(span left, span right) -> span
{
    if(left.file != right.file) {
        return left;
    }

    return span{
        .file = left.file,
        .start = std::min(left.start, right.start),
        .end = std::max(left.end, right.end),
    };
}
