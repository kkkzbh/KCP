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

export using expr_id = syntax_id<expr_tag>;
export using stmt_id = syntax_id<stmt_tag>;
export using type_id = syntax_id<type_tag>;
export using function_id = syntax_id<function_tag>;
