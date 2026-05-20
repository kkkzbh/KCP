export module parser.ast.ids;

import std;

export auto constexpr inline invalid_syntax_id = std::numeric_limits<std::uint32_t>::max();

export template <class Tag>
struct [[nodiscard]] syntax_id
{
    auto constexpr valid() const -> bool
    {
        return value != invalid_syntax_id;
    }

    explicit constexpr operator bool() const
    {
        return valid();
    }

    auto constexpr operator==(syntax_id const&) const -> bool = default;

    std::uint32_t value = invalid_syntax_id;
};

export struct expr_tag;
export struct stmt_tag;
export struct type_tag;
export struct function_tag;
export struct type_alias_tag;
export struct struct_tag;
export struct enum_tag;
export struct variant_tag;
export struct impl_tag;
export struct concept_tag;
export struct concept_impl_tag;

export using expr_id = syntax_id<expr_tag>;
export using stmt_id = syntax_id<stmt_tag>;
export using type_id = syntax_id<type_tag>;
export using function_id = syntax_id<function_tag>;
export using type_alias_id = syntax_id<type_alias_tag>;
export using struct_id = syntax_id<struct_tag>;
export using enum_id = syntax_id<enum_tag>;
export using variant_id = syntax_id<variant_tag>;
export using impl_id = syntax_id<impl_tag>;
export using concept_id = syntax_id<concept_tag>;
export using concept_impl_id = syntax_id<concept_impl_tag>;
