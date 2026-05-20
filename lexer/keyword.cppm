module lexer:keyword;

import std;
import lexer.token;
import lexer.charset;
import :state;

/// @brief 查询标识符文本是否为 cp 关键字。
auto keyword_kind(std::string_view text) -> std::optional<token_kind>
{
    using namespace std::literals;

    switch(text.size()) {
        case 2:
            if(text == "as"sv) {
                return token_kind::kw_as;
            }
            if(text == "do"sv) {
                return token_kind::kw_do;
            }
            if(text == "if"sv) {
                return token_kind::kw_if;
            }
            if(text == "or"sv) {
                return token_kind::kw_or;
            }
            break;
        case 3:
            if(text == "and"sv) {
                return token_kind::kw_and;
            }
            if(text == "new"sv) {
                return token_kind::kw_new;
            }
            if(text == "for"sv) {
                return token_kind::kw_for;
            }
            if(text == "let"sv) {
                return token_kind::kw_let;
            }
            if(text == "ref"sv) {
                return token_kind::kw_ref;
            }
            if(text == "not"sv) {
                return token_kind::kw_not;
            }
            break;
        case 4:
            if(text == "else"sv) {
                return token_kind::kw_else;
            }
            if(text == "enum"sv) {
                return token_kind::kw_enum;
            }
            if(text == "impl"sv) {
                return token_kind::kw_impl;
            }
            if(text == "like"sv) {
                return token_kind::kw_like;
            }
            if(text == "move"sv) {
                return token_kind::kw_move;
            }
            if(text == "true"sv) {
                return token_kind::kw_true;
            }
            break;
        case 5:
            if(text == "break"sv) {
                return token_kind::kw_break;
            }
            if(text == "const"sv) {
                return token_kind::kw_const;
            }
            if(text == "false"sv) {
                return token_kind::kw_false;
            }
            if(text == "while"sv) {
                return token_kind::kw_while;
            }
            break;
        case 7:
            if(text == "concept"sv) {
                return token_kind::kw_concept;
            }
            if(text == "nullptr"sv) {
                return token_kind::kw_nullptr;
            }
            break;
        case 6:
            if(text == "delete"sv) {
                return token_kind::kw_delete;
            }
            if(text == "export"sv) {
                return token_kind::kw_export;
            }
            if(text == "import"sv) {
                return token_kind::kw_import;
            }
            if(text == "module"sv) {
                return token_kind::kw_module;
            }
            if(text == "return"sv) {
                return token_kind::kw_return;
            }
            if(text == "struct"sv) {
                return token_kind::kw_struct;
            }
            break;
        case 8:
            if(text == "continue"sv) {
                return token_kind::kw_continue;
            }
            if(text == "operator"sv) {
                return token_kind::kw_operator;
            }
            break;
        default:
            break;
    }

    return std::nullopt;
}

auto lexer::lex_identifier_or_keyword(token_flags flags) -> token
{
    auto const start = cursor_.offset();
    while(not cursor_.eof() and is_identifier_continue(cursor_.current())) {
        cursor_.advance();
    }

    auto const text = cursor_.source().substr(start, cursor_.offset() - start);
    if(auto const keyword = keyword_kind(text)) {
        return cursor_.make_token(*keyword, start, cursor_.offset(), flags);
    }

    auto token = cursor_.make_token(token_kind::identifier, start, cursor_.offset(), flags);
    token.text = std::string{ text };
    return token;
}
