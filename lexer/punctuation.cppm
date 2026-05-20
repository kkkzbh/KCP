module lexer:punctuation;

import std;
import lexer.token;
import :state;

/// @brief 从当前位置尝试匹配一个 cp 标点 token。
auto lexer::lex_punctuation(token_flags flags) -> std::optional<token>
{
    auto const start = cursor_.offset();
    auto make_token = [&](token_kind kind, std::size_t length) -> token {
        cursor_.advance(length);
        return cursor_.make_token(kind, start, cursor_.offset(), flags);
    };

    switch(cursor_.peek_char()) {
        case '(':
            return make_token(token_kind::l_paren, 1);
        case ')':
            return make_token(token_kind::r_paren, 1);
        case '{':
            return make_token(token_kind::l_brace, 1);
        case '}':
            return make_token(token_kind::r_brace, 1);
        case '[':
            return make_token(token_kind::l_bracket, 1);
        case ']':
            return make_token(token_kind::r_bracket, 1);
        case ',':
            return make_token(token_kind::comma, 1);
        case ';':
            return make_token(token_kind::semicolon, 1);
        case ':':
            if(cursor_.peek_char(1) == ':') {
                return make_token(token_kind::colon_colon, 2);
            }
            return make_token(token_kind::colon, 1);
        case '.':
            return make_token(token_kind::dot, 1);
        case '+':
            if(cursor_.peek_char(1) == '+') {
                return make_token(token_kind::plus_plus, 2);
            }
            if(cursor_.peek_char(1) == '=') {
                return make_token(token_kind::plus_equal, 2);
            }
            return make_token(token_kind::plus, 1);
        case '-':
            if(cursor_.peek_char(1) == '>') {
                return make_token(token_kind::arrow, 2);
            }
            if(cursor_.peek_char(1) == '-') {
                return make_token(token_kind::minus_minus, 2);
            }
            if(cursor_.peek_char(1) == '=') {
                return make_token(token_kind::minus_equal, 2);
            }
            return make_token(token_kind::minus, 1);
        case '*':
            if(cursor_.peek_char(1) == '=') {
                return make_token(token_kind::star_equal, 2);
            }
            return make_token(token_kind::star, 1);
        case '/':
            if(cursor_.peek_char(1) == '=') {
                return make_token(token_kind::slash_equal, 2);
            }
            return make_token(token_kind::slash, 1);
        case '%':
            if(cursor_.peek_char(1) == '=') {
                return make_token(token_kind::percent_equal, 2);
            }
            return make_token(token_kind::percent, 1);
        case '=':
            if(cursor_.peek_char(1) == '=') {
                return make_token(token_kind::equal_equal, 2);
            }
            return make_token(token_kind::equal, 1);
        case '<':
            if(cursor_.peek_char(1) == '<') {
                if(cursor_.peek_char(2) == '=') {
                    return make_token(token_kind::less_less_equal, 3);
                }
                return make_token(token_kind::less_less, 2);
            }
            if(cursor_.peek_char(1) == '=') {
                return make_token(token_kind::less_equal, 2);
            }
            return make_token(token_kind::less, 1);
        case '>':
            if(cursor_.peek_char(1) == '>') {
                if(cursor_.peek_char(2) == '=') {
                    return make_token(token_kind::greater_greater_equal, 3);
                }
                return make_token(token_kind::greater_greater, 2);
            }
            if(cursor_.peek_char(1) == '=') {
                return make_token(token_kind::greater_equal, 2);
            }
            return make_token(token_kind::greater, 1);
        case '&':
            if(cursor_.peek_char(1) == '=') {
                return make_token(token_kind::amp_equal, 2);
            }
            return make_token(token_kind::amp, 1);
        case '|':
            if(cursor_.peek_char(1) == '=') {
                return make_token(token_kind::pipe_equal, 2);
            }
            return make_token(token_kind::pipe, 1);
        case '^':
            if(cursor_.peek_char(1) == '=') {
                return make_token(token_kind::caret_equal, 2);
            }
            return make_token(token_kind::caret, 1);
        case '!':
            if(cursor_.peek_char(1) == '=') {
                return make_token(token_kind::bang_equal, 2);
            }
            return make_token(token_kind::bang, 1);
        case '~':
            return make_token(token_kind::tilde, 1);
        case '?':
            return make_token(token_kind::question, 1);
        default:
            return std::nullopt;
    }
}
