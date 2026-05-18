export module parser.ast.expr;

import std;
import source;
import lexer.token;
import parser.ast.ids;
import parser.ast.type;

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
    std::vector<type_argument_syntax> type_arguments{};
    std::vector<expr_id> arguments{};
};

export struct member_expr_syntax
{
    auto constexpr operator==(member_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    expr_id object{};
    source_span name{};
};

export struct index_expr_syntax
{
    auto constexpr operator==(index_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    expr_id object{};
    expr_id index{};
};

export struct associated_name_expr_syntax
{
    auto constexpr operator==(associated_name_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    type_id type{};
    source_span name{};
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

export struct named_field_initializer_syntax
{
    named_field_initializer_syntax() = default;

    named_field_initializer_syntax(source_span initializer_span, source_span field_name, expr_id initializer_value) :
        full_span(initializer_span),
        name(field_name),
        value(initializer_value) {}

    auto constexpr operator==(named_field_initializer_syntax const& other) const -> bool = default;

    source_span full_span{};
    source_span name{};
    expr_id value{};
};

export struct positional_initializer_syntax
{
    positional_initializer_syntax() = default;

    positional_initializer_syntax(source_span initializer_span, expr_id initializer_value) :
        full_span(initializer_span),
        value(initializer_value) {}

    auto constexpr operator==(positional_initializer_syntax const& other) const -> bool = default;

    source_span full_span{};
    expr_id value{};
};

export using struct_initializer_syntax = std::variant<
    named_field_initializer_syntax,
    positional_initializer_syntax>;

export struct struct_init_expr_syntax
{
    auto constexpr operator==(struct_init_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    type_id type{};
    std::vector<struct_initializer_syntax> initializers{};
};

export struct block_expr_syntax
{
    auto constexpr operator==(block_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::vector<stmt_id> statements{};
    std::optional<expr_id> tail{};
};

export struct match_case_pattern_syntax
{
    auto constexpr operator==(match_case_pattern_syntax const& other) const -> bool = default;

    source_span full_span{};
    source_span name{};
    std::vector<source_span> bindings{};
};

export struct match_wildcard_pattern_syntax
{
    auto constexpr operator==(match_wildcard_pattern_syntax const& other) const -> bool = default;

    source_span full_span{};
};

export using match_pattern_syntax = std::variant<
    match_case_pattern_syntax,
    match_wildcard_pattern_syntax>;

export struct match_arm_syntax
{
    auto constexpr operator==(match_arm_syntax const& other) const -> bool = default;

    source_span full_span{};
    match_pattern_syntax pattern{};
    expr_id value{};
};

export struct match_expr_syntax
{
    auto constexpr operator==(match_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    expr_id value{};
    std::vector<match_arm_syntax> arms{};
};

export struct lambda_expr_syntax
{
    auto constexpr operator==(lambda_expr_syntax const& other) const -> bool = default;

    source_span full_span{};
    function_id function{};
};

export using expr_syntax = std::variant<
    name_expr_syntax,
    literal_expr_syntax,
    unary_expr_syntax,
    binary_expr_syntax,
    assignment_expr_syntax,
    call_expr_syntax,
    member_expr_syntax,
    index_expr_syntax,
    associated_name_expr_syntax,
    cast_expr_syntax,
    array_literal_expr_syntax,
    tuple_literal_expr_syntax,
    grouped_expr_syntax,
    struct_init_expr_syntax,
    block_expr_syntax,
    match_expr_syntax,
    lambda_expr_syntax>;
