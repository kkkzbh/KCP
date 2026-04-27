/// @brief 词法识别用的两张数据表：标点与关键字。
/// @details 两者同为"把一段拼写映射到 `token_kind`"的数据驱动识别；放在一起便于统一维护。
export module lexer.tables;

import std;
import lexer.token;

using namespace std::literals;

/// @brief 所有标点 token 的拼写到枚举映射，按最长优先排列。
export auto constexpr punctuation_table = std::to_array({
    std::pair{ "<<="sv, token_kind::less_less_equal },
    std::pair{ ">>="sv, token_kind::greater_greater_equal },
    std::pair{ "::"sv,  token_kind::colon_colon },
    std::pair{ "->"sv,  token_kind::arrow },
    std::pair{ "=="sv,  token_kind::equal_equal },
    std::pair{ "!="sv,  token_kind::bang_equal },
    std::pair{ "<="sv,  token_kind::less_equal },
    std::pair{ ">="sv,  token_kind::greater_equal },
    std::pair{ "<<"sv,  token_kind::less_less },
    std::pair{ ">>"sv,  token_kind::greater_greater },
    std::pair{ "++"sv,  token_kind::plus_plus },
    std::pair{ "--"sv,  token_kind::minus_minus },
    std::pair{ "+="sv,  token_kind::plus_equal },
    std::pair{ "-="sv,  token_kind::minus_equal },
    std::pair{ "*="sv,  token_kind::star_equal },
    std::pair{ "/="sv,  token_kind::slash_equal },
    std::pair{ "%="sv,  token_kind::percent_equal },
    std::pair{ "&="sv,  token_kind::amp_equal },
    std::pair{ "|="sv,  token_kind::pipe_equal },
    std::pair{ "^="sv,  token_kind::caret_equal },
    std::pair{ "("sv,   token_kind::l_paren },
    std::pair{ ")"sv,   token_kind::r_paren },
    std::pair{ "{"sv,   token_kind::l_brace },
    std::pair{ "}"sv,   token_kind::r_brace },
    std::pair{ "["sv,   token_kind::l_bracket },
    std::pair{ "]"sv,   token_kind::r_bracket },
    std::pair{ ","sv,   token_kind::comma },
    std::pair{ ";"sv,   token_kind::semicolon },
    std::pair{ ":"sv,   token_kind::colon },
    std::pair{ "."sv,   token_kind::dot },
    std::pair{ "+"sv,   token_kind::plus },
    std::pair{ "-"sv,   token_kind::minus },
    std::pair{ "*"sv,   token_kind::star },
    std::pair{ "/"sv,   token_kind::slash },
    std::pair{ "%"sv,   token_kind::percent },
    std::pair{ "="sv,   token_kind::equal },
    std::pair{ "<"sv,   token_kind::less },
    std::pair{ ">"sv,   token_kind::greater },
    std::pair{ "&"sv,   token_kind::amp },
    std::pair{ "|"sv,   token_kind::pipe },
    std::pair{ "^"sv,   token_kind::caret },
    std::pair{ "~"sv,   token_kind::tilde },
    std::pair{ "?"sv,   token_kind::question },
});

/// @brief 对输入的前缀做最长匹配，返回命中的 token 及其消耗长度。
export auto match_punctuation(std::string_view text) -> std::optional<std::pair<token_kind, std::size_t>>
{
    for(auto const& [spelling, kind] : punctuation_table) {
        if(text.starts_with(spelling)) {
            return std::pair{ kind, spelling.size() };
        }
    }
    return std::nullopt;
}

/// @brief 所有关键字的拼写到枚举映射。
export auto constexpr keyword_table = std::to_array({
    std::pair{ "let"sv,      token_kind::kw_let },
    std::pair{ "const"sv,    token_kind::kw_const },
    std::pair{ "if"sv,       token_kind::kw_if },
    std::pair{ "else"sv,     token_kind::kw_else },
    std::pair{ "while"sv,    token_kind::kw_while },
    std::pair{ "do"sv,       token_kind::kw_do },
    std::pair{ "for"sv,      token_kind::kw_for },
    std::pair{ "break"sv,    token_kind::kw_break },
    std::pair{ "continue"sv, token_kind::kw_continue },
    std::pair{ "return"sv,   token_kind::kw_return },
    std::pair{ "import"sv,   token_kind::kw_import },
    std::pair{ "export"sv,   token_kind::kw_export },
    std::pair{ "module"sv,   token_kind::kw_module },
    std::pair{ "struct"sv,   token_kind::kw_struct },
    std::pair{ "impl"sv,     token_kind::kw_impl },
    std::pair{ "trait"sv,    token_kind::kw_trait },
    std::pair{ "as"sv,       token_kind::kw_as },
    std::pair{ "true"sv,     token_kind::kw_true },
    std::pair{ "false"sv,    token_kind::kw_false },
    std::pair{ "and"sv,      token_kind::kw_and },
    std::pair{ "or"sv,       token_kind::kw_or },
    std::pair{ "not"sv,      token_kind::kw_not },
});

/// @brief 将标识符文本解析为关键字 `token_kind`。
/// @return 若匹配到关键字，返回对应的 `token_kind`；否则返回 `std::nullopt`。
export auto keyword_kind(std::string_view text) -> std::optional<token_kind>
{
    for(auto const& [keyword, kind] : keyword_table) {
        if(keyword == text) {
            return kind;
        }
    }
    return std::nullopt;
}
