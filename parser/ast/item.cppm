export module parser.ast.item;

import std;
import source;
import parser.ast.ids;
import parser.ast.name;

export struct parameter_syntax
{
    auto constexpr operator==(parameter_syntax const& other) const -> bool = default;

    source_span full_span{};
    bool is_const{};
    source_span name{};
    type_id type{};
};

export struct function_syntax
{
    auto constexpr operator==(function_syntax const& other) const -> bool = default;

    source_span full_span{};
    bool exported{};
    source_span name{};
    std::vector<parameter_syntax> parameters{};
    std::optional<type_id> return_type{};
    stmt_id body{};
};

export struct import_syntax
{
    auto constexpr operator==(import_syntax const& other) const -> bool = default;

    source_span full_span{};
    module_name_syntax name{};
};

export struct module_header_syntax
{
    auto constexpr operator==(module_header_syntax const& other) const -> bool = default;

    source_span full_span{};
    bool exported{};
    module_name_syntax name{};
};

export struct translation_unit_syntax
{
    auto constexpr operator==(translation_unit_syntax const& other) const -> bool = default;

    source_span full_span{};
    std::optional<module_header_syntax> module_header{};
    std::vector<import_syntax> imports{};
    std::vector<function_id> functions{};
};
