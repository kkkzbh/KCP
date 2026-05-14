export module parser.expression;

import std;
import lexer.token;

export struct binary_operator_entry
{
    token_kind kind;
    std::string_view terminal_name;
    int left_bp;
    int right_bp;
};

auto constexpr make_binary_operator_entry(
    token_kind kind,
    std::string_view terminal_name,
    int left_bp,
    int right_bp
) -> binary_operator_entry
{
    return {
        .kind = kind,
        .terminal_name = terminal_name,
        .left_bp = left_bp,
        .right_bp = right_bp,
    };
}

// Source of truth for cp binary operator precedence shared by:
//   * the Pratt expression parser in parser.syntax
//   * the operator-precedence experiment in parser.op
// kw_as participates in precedence comparisons but its right-hand side is a
// Type, not an expression, so the Pratt loop special-cases it after the binding
// lookup.
export auto constexpr inline cp_binary_operator_table = std::array<binary_operator_entry, 19uz>{{
    make_binary_operator_entry(token_kind::kw_or, "kw_or", 30, 31),
    make_binary_operator_entry(token_kind::kw_and, "kw_and", 40, 41),
    make_binary_operator_entry(token_kind::pipe, "pipe", 50, 51),
    make_binary_operator_entry(token_kind::caret, "caret", 60, 61),
    make_binary_operator_entry(token_kind::amp, "amp", 70, 71),
    make_binary_operator_entry(token_kind::equal_equal, "equal_equal", 80, 81),
    make_binary_operator_entry(token_kind::bang_equal, "bang_equal", 80, 81),
    make_binary_operator_entry(token_kind::less, "less", 90, 91),
    make_binary_operator_entry(token_kind::less_equal, "less_equal", 90, 91),
    make_binary_operator_entry(token_kind::greater, "greater", 90, 91),
    make_binary_operator_entry(token_kind::greater_equal, "greater_equal", 90, 91),
    make_binary_operator_entry(token_kind::less_less, "less_less", 100, 101),
    make_binary_operator_entry(token_kind::greater_greater, "greater_greater", 100, 101),
    make_binary_operator_entry(token_kind::plus, "plus", 110, 111),
    make_binary_operator_entry(token_kind::minus, "minus", 110, 111),
    make_binary_operator_entry(token_kind::star, "star", 120, 121),
    make_binary_operator_entry(token_kind::slash, "slash", 120, 121),
    make_binary_operator_entry(token_kind::percent, "percent", 120, 121),
    make_binary_operator_entry(token_kind::kw_as, "kw_as", 130, 131),
}};

export auto constexpr find_binary_operator(token_kind kind) -> std::optional<binary_operator_entry>
{
    auto entry = std::ranges::find(cp_binary_operator_table, kind, &binary_operator_entry::kind);
    if(entry == cp_binary_operator_table.end()) {
        return std::nullopt;
    }
    return *entry;
}
