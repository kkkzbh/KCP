export module parser.ast.type;

import std;
import source;
import lexer.token;
import parser.ast.ids;

export struct type_argument_type_syntax
{
    auto constexpr operator==(type_argument_type_syntax const& other) const -> bool = default;

    type_id type{};
};

export struct type_argument_literal_syntax
{
    auto constexpr operator==(type_argument_literal_syntax const& other) const -> bool = default;

    source_span literal{};
};

export struct type_argument_name_syntax
{
    auto constexpr operator==(type_argument_name_syntax const& other) const -> bool = default;

    source_span name{};
};

export using type_argument_syntax = std::variant<
    type_argument_type_syntax,
    type_argument_literal_syntax,
    type_argument_name_syntax>;

export struct function_type_parameter_syntax
{
    auto constexpr operator==(function_type_parameter_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::optional<source_span> name{};
    type_id type{};
};

export struct type_syntax
{
    auto constexpr operator==(type_syntax const& other) const -> bool = default;

    source_span full_span{};
    source_span name{};
    bool is_function_type{};
    bool is_function_pointer{};
    std::vector<function_type_parameter_syntax> function_parameters{};
    type_id function_return{};
    bool is_decltype{};
    expr_id decltype_expression{};
    bool is_never_type{};
    bool is_array_type{};
    type_id array_element{};
    type_argument_syntax array_length{};
    bool is_tuple_type{};
    bool is_grouped_type{};
    type_id grouped_type{};
    std::vector<type_id> tuple_elements{};
    std::vector<type_argument_syntax> arguments{};
    std::vector<source_span> associated_names{};
    bool is_const{};
    bool is_like{};
    bool is_forward{};
    std::vector<token_kind> suffix_operators{};
};
