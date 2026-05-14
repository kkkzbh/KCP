export module parser.ast.expr;

import std;
import source;
import lexer.token;
import parser.ast.ids;

export struct name_expr_syntax
{
    auto constexpr operator==(name_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    source_span name{};
};

export struct literal_expr_syntax
{
    auto constexpr operator==(literal_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
};

export enum class unary_position {
    prefix,
    postfix,
};

export struct unary_expr_syntax
{
    auto constexpr operator==(unary_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    token_kind operator_kind{token_kind::eof};
    unary_position position{unary_position::prefix};
    expr_id operand{};
};

export struct binary_expr_syntax
{
    auto constexpr operator==(binary_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    token_kind operator_kind{token_kind::eof};
    expr_id left{};
    expr_id right{};
};

export struct assignment_expr_syntax
{
    auto constexpr operator==(assignment_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    token_kind operator_kind{token_kind::eof};
    expr_id left{};
    expr_id right{};
};

export struct call_expr_syntax
{
    auto constexpr operator==(call_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    expr_id callee{};
    std::vector<expr_id> arguments{};
};

export struct cast_expr_syntax
{
    auto constexpr operator==(cast_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    expr_id operand{};
    type_id type{};
};

export struct array_literal_expr_syntax
{
    auto constexpr operator==(array_literal_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::vector<expr_id> elements{};
};

export struct sequence_literal_expr_syntax
{
    auto constexpr operator==(sequence_literal_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::vector<expr_id> elements{};
};

export struct tuple_literal_expr_syntax
{
    auto constexpr operator==(tuple_literal_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::vector<expr_id> elements{};
};

export struct grouped_expr_syntax
{
    auto constexpr operator==(grouped_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    expr_id expression{};
};

export using expr_syntax = std::variant<
    name_expr_syntax,
    literal_expr_syntax,
    unary_expr_syntax,
    binary_expr_syntax,
    assignment_expr_syntax,
    call_expr_syntax,
    cast_expr_syntax,
    array_literal_expr_syntax,
    sequence_literal_expr_syntax,
    tuple_literal_expr_syntax,
    grouped_expr_syntax>;
