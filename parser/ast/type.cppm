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

export using type_argument_syntax = std::variant<
    type_argument_type_syntax,
    type_argument_literal_syntax>;

export struct type_syntax
{
    auto constexpr operator==(type_syntax const& other) const -> bool = default;

    source_span full_span{};
    source_span name{};
    std::vector<type_argument_syntax> arguments{};
    bool is_const{};
    std::vector<token_kind> suffix_operators{};
};
