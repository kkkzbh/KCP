module lexer:punctuation;

import std;
import lexer.token;
import :state;

/// @brief 从当前位置尝试匹配一个 cp 标点 token。
auto lexer::lex_punctuation(token_flags flags) -> std::optional<token>
{
    auto const start = cursor_.offset();

    switch(cursor_.peek_char()) {
        case '(':
            return make_punctuation_token(token_kind::l_paren, start, 1, flags);
        case ')':
            return make_punctuation_token(token_kind::r_paren, start, 1, flags);
        case '{':
            return make_punctuation_token(token_kind::l_brace, start, 1, flags);
        case '}':
            return make_punctuation_token(token_kind::r_brace, start, 1, flags);
        case '[':
            return make_punctuation_token(token_kind::l_bracket, start, 1, flags);
        case ']':
            return make_punctuation_token(token_kind::r_bracket, start, 1, flags);
        case ',':
            return make_punctuation_token(token_kind::comma, start, 1, flags);
        case ';':
            return make_punctuation_token(token_kind::semicolon, start, 1, flags);
        case ':':
            if(cursor_.peek_char(1) == ':') {
                return make_punctuation_token(token_kind::colon_colon, start, 2, flags);
            }
            return make_punctuation_token(token_kind::colon, start, 1, flags);
        case '.':
            return make_punctuation_token(token_kind::dot, start, 1, flags);
        case '+':
            if(cursor_.peek_char(1) == '+') {
                return make_punctuation_token(token_kind::plus_plus, start, 2, flags);
            }
            if(cursor_.peek_char(1) == '=') {
                return make_punctuation_token(token_kind::plus_equal, start, 2, flags);
            }
            return make_punctuation_token(token_kind::plus, start, 1, flags);
        case '-':
            if(cursor_.peek_char(1) == '>') {
                return make_punctuation_token(token_kind::arrow, start, 2, flags);
            }
            if(cursor_.peek_char(1) == '-') {
                return make_punctuation_token(token_kind::minus_minus, start, 2, flags);
            }
            if(cursor_.peek_char(1) == '=') {
                return make_punctuation_token(token_kind::minus_equal, start, 2, flags);
            }
            return make_punctuation_token(token_kind::minus, start, 1, flags);
        case '*':
            if(cursor_.peek_char(1) == '=') {
                return make_punctuation_token(token_kind::star_equal, start, 2, flags);
            }
            return make_punctuation_token(token_kind::star, start, 1, flags);
        case '/':
            if(cursor_.peek_char(1) == '=') {
                return make_punctuation_token(token_kind::slash_equal, start, 2, flags);
            }
            return make_punctuation_token(token_kind::slash, start, 1, flags);
        case '%':
            if(cursor_.peek_char(1) == '=') {
                return make_punctuation_token(token_kind::percent_equal, start, 2, flags);
            }
            return make_punctuation_token(token_kind::percent, start, 1, flags);
        case '=':
            if(cursor_.peek_char(1) == '=') {
                return make_punctuation_token(token_kind::equal_equal, start, 2, flags);
            }
            return make_punctuation_token(token_kind::equal, start, 1, flags);
        case '<':
            if(cursor_.peek_char(1) == '<') {
                if(cursor_.peek_char(2) == '=') {
                    return make_punctuation_token(token_kind::less_less_equal, start, 3, flags);
                }
                return make_punctuation_token(token_kind::less_less, start, 2, flags);
            }
            if(cursor_.peek_char(1) == '=') {
                return make_punctuation_token(token_kind::less_equal, start, 2, flags);
            }
            return make_punctuation_token(token_kind::less, start, 1, flags);
        case '>':
            if(cursor_.peek_char(1) == '>') {
                if(cursor_.peek_char(2) == '=') {
                    return make_punctuation_token(token_kind::greater_greater_equal, start, 3, flags);
                }
                return make_punctuation_token(token_kind::greater_greater, start, 2, flags);
            }
            if(cursor_.peek_char(1) == '=') {
                return make_punctuation_token(token_kind::greater_equal, start, 2, flags);
            }
            return make_punctuation_token(token_kind::greater, start, 1, flags);
        case '&':
            if(cursor_.peek_char(1) == '=') {
                return make_punctuation_token(token_kind::amp_equal, start, 2, flags);
            }
            return make_punctuation_token(token_kind::amp, start, 1, flags);
        case '|':
            if(cursor_.peek_char(1) == '=') {
                return make_punctuation_token(token_kind::pipe_equal, start, 2, flags);
            }
            return make_punctuation_token(token_kind::pipe, start, 1, flags);
        case '^':
            if(cursor_.peek_char(1) == '=') {
                return make_punctuation_token(token_kind::caret_equal, start, 2, flags);
            }
            return make_punctuation_token(token_kind::caret, start, 1, flags);
        case '!':
            if(cursor_.peek_char(1) == '=') {
                return make_punctuation_token(token_kind::bang_equal, start, 2, flags);
            }
            return std::nullopt;
        case '~':
            return make_punctuation_token(token_kind::tilde, start, 1, flags);
        case '?':
            return make_punctuation_token(token_kind::question, start, 1, flags);
        default:
            return std::nullopt;
    }
}
