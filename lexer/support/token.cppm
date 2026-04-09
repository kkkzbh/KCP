export module lexer.token;

import std;
import lexer.source;

export enum class token_kind
{
    eof,
    invalid,

    identifier,
    integer_literal,
    float_literal,
    char_literal,
    string_literal,

    kw_let,
    kw_const,
    kw_if,
    kw_else,
    kw_while,
    kw_do,
    kw_for,
    kw_break,
    kw_continue,
    kw_return,
    kw_import,
    kw_export,
    kw_module,
    kw_struct,
    kw_impl,
    kw_trait,
    kw_as,
    kw_true,
    kw_false,
    kw_and,
    kw_or,
    kw_not,

    l_paren,
    r_paren,
    l_brace,
    r_brace,
    l_bracket,
    r_bracket,
    comma,
    semicolon,
    colon,
    colon_colon,
    dot,
    arrow,
    plus,
    plus_equal,
    minus,
    minus_equal,
    star,
    star_equal,
    slash,
    slash_equal,
    percent,
    percent_equal,
    equal,
    equal_equal,
    bang_equal,
    less,
    less_equal,
    greater,
    greater_equal,
    amp,
    amp_equal,
    pipe,
    pipe_equal,
    caret,
    caret_equal,
    tilde,
    less_less,
    less_less_equal,
    greater_greater,
    greater_greater_equal,
    plus_plus,
    minus_minus,
    question,
};

export [[nodiscard]] auto to_string(token_kind kind) -> std::string_view;

export enum class token_flags : std::uint8_t
{
    none = 0,
    leading_space = 1 << 0,
    start_of_line = 1 << 1,
    unterminated = 1 << 2,
    recovered = 1 << 3,
};

export [[nodiscard]] constexpr auto operator|(token_flags lhs, token_flags rhs) -> token_flags
{
    return static_cast<token_flags>(
        static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

export constexpr auto operator|=(token_flags& lhs, token_flags rhs) -> token_flags&
{
    lhs = lhs | rhs;
    return lhs;
}

export [[nodiscard]] constexpr auto has_flag(token_flags flags, token_flags bit) -> bool
{
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(bit)) != 0;
}

export struct token
{
    token_kind kind{};
    span source_span{};
    token_flags flags{token_flags::none};

    [[nodiscard]] constexpr auto operator==(token const&) const -> bool = default;
};

auto to_string(token_kind kind) -> std::string_view
{
    using enum token_kind;

    switch (kind) {
    case eof: return "eof";
    case invalid: return "invalid";
    case identifier: return "identifier";
    case integer_literal: return "integer_literal";
    case float_literal: return "float_literal";
    case char_literal: return "char_literal";
    case string_literal: return "string_literal";
    case kw_let: return "kw_let";
    case kw_const: return "kw_const";
    case kw_if: return "kw_if";
    case kw_else: return "kw_else";
    case kw_while: return "kw_while";
    case kw_do: return "kw_do";
    case kw_for: return "kw_for";
    case kw_break: return "kw_break";
    case kw_continue: return "kw_continue";
    case kw_return: return "kw_return";
    case kw_import: return "kw_import";
    case kw_export: return "kw_export";
    case kw_module: return "kw_module";
    case kw_struct: return "kw_struct";
    case kw_impl: return "kw_impl";
    case kw_trait: return "kw_trait";
    case kw_as: return "kw_as";
    case kw_true: return "kw_true";
    case kw_false: return "kw_false";
    case kw_and: return "kw_and";
    case kw_or: return "kw_or";
    case kw_not: return "kw_not";
    case l_paren: return "l_paren";
    case r_paren: return "r_paren";
    case l_brace: return "l_brace";
    case r_brace: return "r_brace";
    case l_bracket: return "l_bracket";
    case r_bracket: return "r_bracket";
    case comma: return "comma";
    case semicolon: return "semicolon";
    case colon: return "colon";
    case colon_colon: return "colon_colon";
    case dot: return "dot";
    case arrow: return "arrow";
    case plus: return "plus";
    case plus_equal: return "plus_equal";
    case minus: return "minus";
    case minus_equal: return "minus_equal";
    case star: return "star";
    case star_equal: return "star_equal";
    case slash: return "slash";
    case slash_equal: return "slash_equal";
    case percent: return "percent";
    case percent_equal: return "percent_equal";
    case equal: return "equal";
    case equal_equal: return "equal_equal";
    case bang_equal: return "bang_equal";
    case less: return "less";
    case less_equal: return "less_equal";
    case greater: return "greater";
    case greater_equal: return "greater_equal";
    case amp: return "amp";
    case amp_equal: return "amp_equal";
    case pipe: return "pipe";
    case pipe_equal: return "pipe_equal";
    case caret: return "caret";
    case caret_equal: return "caret_equal";
    case tilde: return "tilde";
    case less_less: return "less_less";
    case less_less_equal: return "less_less_equal";
    case greater_greater: return "greater_greater";
    case greater_greater_equal: return "greater_greater_equal";
    case plus_plus: return "plus_plus";
    case minus_minus: return "minus_minus";
    case question: return "question";
    }

    return "unknown";
}
