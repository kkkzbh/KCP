export module parser.ast.stmt;

import std;
import source;
import parser.ast.ids;

export struct block_statement_syntax
{
    auto constexpr operator==(block_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::vector<stmt_id> statements{};
};

export struct declaration_statement_syntax
{
    auto constexpr operator==(declaration_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    bool is_const{};
    source_span name{};
    std::optional<type_id> declared_type{};
    expr_id initializer{};
};

export struct if_statement_syntax
{
    auto constexpr operator==(if_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    expr_id condition{};
    stmt_id then_branch{};
    std::optional<stmt_id> else_branch{};
};

export struct while_statement_syntax
{
    auto constexpr operator==(while_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    expr_id condition{};
    stmt_id body{};
};

export struct do_while_statement_syntax
{
    auto constexpr operator==(do_while_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    stmt_id body{};
    expr_id condition{};
};

export struct for_statement_syntax
{
    auto constexpr operator==(for_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    bool is_const{};
    source_span name{};
    std::optional<source_span> label{};
    expr_id range{};
    stmt_id body{};
};

export struct break_statement_syntax
{
    auto constexpr operator==(break_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::optional<source_span> label{};
};

export struct continue_statement_syntax
{
    auto constexpr operator==(continue_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::optional<source_span> label{};
};

export struct return_statement_syntax
{
    auto constexpr operator==(return_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::optional<expr_id> value{};
};

export struct expression_statement_syntax
{
    auto constexpr operator==(expression_statement_syntax const& other) const -> bool = default;

    source_span full_span{};
    expr_id expression{};
};

export using statement_syntax = std::variant<
    block_statement_syntax,
    declaration_statement_syntax,
    if_statement_syntax,
    while_statement_syntax,
    do_while_statement_syntax,
    for_statement_syntax,
    break_statement_syntax,
    continue_statement_syntax,
    return_statement_syntax,
    expression_statement_syntax>;
