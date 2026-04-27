export module parser.expression_precedence;

import std;
import lexer.token;

export struct binary_operator_entry
{
    token_kind kind;
    std::string_view terminal_name;
    int left_bp;
    int right_bp;
};

// Source of truth for cp binary operator precedence shared by:
//   * the Pratt expression parser in parser.recursive_descent
//   * the operator-precedence experiment in parser.operator_precedence
// kw_as participates in precedence comparisons but its right-hand side is a Type,
// not an expression, so the Pratt loop special-cases it after the binding lookup.
export inline constexpr auto cp_binary_operator_table = std::array<binary_operator_entry, 19uz>{ {
    { token_kind::kw_or,           "kw_or",           30,  31  },
    { token_kind::kw_and,          "kw_and",          40,  41  },
    { token_kind::pipe,            "pipe",            50,  51  },
    { token_kind::caret,           "caret",           60,  61  },
    { token_kind::amp,             "amp",             70,  71  },
    { token_kind::equal_equal,     "equal_equal",     80,  81  },
    { token_kind::bang_equal,      "bang_equal",      80,  81  },
    { token_kind::less,            "less",            90,  91  },
    { token_kind::less_equal,      "less_equal",      90,  91  },
    { token_kind::greater,         "greater",         90,  91  },
    { token_kind::greater_equal,   "greater_equal",   90,  91  },
    { token_kind::less_less,       "less_less",       100, 101 },
    { token_kind::greater_greater, "greater_greater", 100, 101 },
    { token_kind::plus,            "plus",            110, 111 },
    { token_kind::minus,           "minus",           110, 111 },
    { token_kind::star,            "star",            120, 121 },
    { token_kind::slash,           "slash",           120, 121 },
    { token_kind::percent,         "percent",         120, 121 },
    { token_kind::kw_as,           "kw_as",           130, 131 },
} };

export [[nodiscard]] constexpr auto find_binary_operator(token_kind kind)
    -> std::optional<binary_operator_entry>
{
    for(auto const& entry : cp_binary_operator_table) {
        if(entry.kind == kind) {
            return entry;
        }
    }
    return std::nullopt;
}
