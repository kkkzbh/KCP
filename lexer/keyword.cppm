export module lexer.keyword;

import std;
import lexer.token;

/// @brief 查询标识符文本是否为 cp 关键字。
export auto keyword_kind(std::string_view text) -> std::optional<token_kind>
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
            if(text == "for"sv) {
                return token_kind::kw_for;
            }
            if(text == "let"sv) {
                return token_kind::kw_let;
            }
            if(text == "not"sv) {
                return token_kind::kw_not;
            }
            break;
        case 4:
            if(text == "else"sv) {
                return token_kind::kw_else;
            }
            if(text == "impl"sv) {
                return token_kind::kw_impl;
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
            break;
        case 6:
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
            break;
        default:
            break;
    }

    return std::nullopt;
}
