export module lexer.keyword;

import lexer.token;
import std.text.str;

export keyword_kind(text: str) -> token_kind
{
    if(text == "int") { return token_kind::kw_int; }
    if(text == "void") { return token_kind::kw_void; }
    if(text == "if") { return token_kind::kw_if; }
    if(text == "else") { return token_kind::kw_else; }
    if(text == "while") { return token_kind::kw_while; }
    if(text == "return") { return token_kind::kw_return; }
    return token_kind::identifier;
}
