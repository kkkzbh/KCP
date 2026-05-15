export module lexer.punctuation;

import std;
import lexer.token;

/// @brief 标点最长匹配结果。
export struct punctuation_match
{
    token_kind kind{};
    std::size_t length{};
};

auto char_at(std::string_view source, std::size_t start, std::size_t distance = 0) -> char
{
    auto const index = start + distance;
    return index < source.size() ? source[index] : '\0';
}

/// @brief 从 `start` 开始尝试匹配一个 cp 标点 token。
export auto match_punctuation(std::string_view source, std::size_t start) -> std::optional<punctuation_match>
{
    switch(char_at(source, start)) {
        case '(':
            return punctuation_match{ .kind = token_kind::l_paren, .length = 1 };
        case ')':
            return punctuation_match{ .kind = token_kind::r_paren, .length = 1 };
        case '{':
            return punctuation_match{ .kind = token_kind::l_brace, .length = 1 };
        case '}':
            return punctuation_match{ .kind = token_kind::r_brace, .length = 1 };
        case '[':
            return punctuation_match{ .kind = token_kind::l_bracket, .length = 1 };
        case ']':
            return punctuation_match{ .kind = token_kind::r_bracket, .length = 1 };
        case ',':
            return punctuation_match{ .kind = token_kind::comma, .length = 1 };
        case ';':
            return punctuation_match{ .kind = token_kind::semicolon, .length = 1 };
        case ':':
            if(char_at(source, start, 1) == ':') {
                return punctuation_match{ .kind = token_kind::colon_colon, .length = 2 };
            }
            return punctuation_match{ .kind = token_kind::colon, .length = 1 };
        case '.':
            return punctuation_match{ .kind = token_kind::dot, .length = 1 };
        case '+':
            if(char_at(source, start, 1) == '+') {
                return punctuation_match{ .kind = token_kind::plus_plus, .length = 2 };
            }
            if(char_at(source, start, 1) == '=') {
                return punctuation_match{ .kind = token_kind::plus_equal, .length = 2 };
            }
            return punctuation_match{ .kind = token_kind::plus, .length = 1 };
        case '-':
            if(char_at(source, start, 1) == '>') {
                return punctuation_match{ .kind = token_kind::arrow, .length = 2 };
            }
            if(char_at(source, start, 1) == '-') {
                return punctuation_match{ .kind = token_kind::minus_minus, .length = 2 };
            }
            if(char_at(source, start, 1) == '=') {
                return punctuation_match{ .kind = token_kind::minus_equal, .length = 2 };
            }
            return punctuation_match{ .kind = token_kind::minus, .length = 1 };
        case '*':
            if(char_at(source, start, 1) == '=') {
                return punctuation_match{ .kind = token_kind::star_equal, .length = 2 };
            }
            return punctuation_match{ .kind = token_kind::star, .length = 1 };
        case '/':
            if(char_at(source, start, 1) == '=') {
                return punctuation_match{ .kind = token_kind::slash_equal, .length = 2 };
            }
            return punctuation_match{ .kind = token_kind::slash, .length = 1 };
        case '%':
            if(char_at(source, start, 1) == '=') {
                return punctuation_match{ .kind = token_kind::percent_equal, .length = 2 };
            }
            return punctuation_match{ .kind = token_kind::percent, .length = 1 };
        case '=':
            if(char_at(source, start, 1) == '=') {
                return punctuation_match{ .kind = token_kind::equal_equal, .length = 2 };
            }
            return punctuation_match{ .kind = token_kind::equal, .length = 1 };
        case '<':
            if(char_at(source, start, 1) == '<') {
                if(char_at(source, start, 2) == '=') {
                    return punctuation_match{ .kind = token_kind::less_less_equal, .length = 3 };
                }
                return punctuation_match{ .kind = token_kind::less_less, .length = 2 };
            }
            if(char_at(source, start, 1) == '=') {
                return punctuation_match{ .kind = token_kind::less_equal, .length = 2 };
            }
            return punctuation_match{ .kind = token_kind::less, .length = 1 };
        case '>':
            if(char_at(source, start, 1) == '>') {
                if(char_at(source, start, 2) == '=') {
                    return punctuation_match{ .kind = token_kind::greater_greater_equal, .length = 3 };
                }
                return punctuation_match{ .kind = token_kind::greater_greater, .length = 2 };
            }
            if(char_at(source, start, 1) == '=') {
                return punctuation_match{ .kind = token_kind::greater_equal, .length = 2 };
            }
            return punctuation_match{ .kind = token_kind::greater, .length = 1 };
        case '&':
            if(char_at(source, start, 1) == '=') {
                return punctuation_match{ .kind = token_kind::amp_equal, .length = 2 };
            }
            return punctuation_match{ .kind = token_kind::amp, .length = 1 };
        case '|':
            if(char_at(source, start, 1) == '=') {
                return punctuation_match{ .kind = token_kind::pipe_equal, .length = 2 };
            }
            return punctuation_match{ .kind = token_kind::pipe, .length = 1 };
        case '^':
            if(char_at(source, start, 1) == '=') {
                return punctuation_match{ .kind = token_kind::caret_equal, .length = 2 };
            }
            return punctuation_match{ .kind = token_kind::caret, .length = 1 };
        case '!':
            if(char_at(source, start, 1) == '=') {
                return punctuation_match{ .kind = token_kind::bang_equal, .length = 2 };
            }
            return std::nullopt;
        case '~':
            return punctuation_match{ .kind = token_kind::tilde, .length = 1 };
        case '?':
            return punctuation_match{ .kind = token_kind::question, .length = 1 };
        default:
            return std::nullopt;
    }
}
