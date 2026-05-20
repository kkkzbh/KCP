export module lexer.keyword;

import lexer.token;

same_text(text: str, expected: str) -> bool
{
    if(text.size() != expected.size()) {
        return false;
    }
    let index: usize = 0;
    while(index < text.size()) {
        if(text[index] != expected[index]) {
            return false;
        }
        ++index;
    }
    return true;
}

export keyword_kind(text: str) -> token_kind
{
    if(same_text(text, "int")) { return token_kind::kw_int; }
    if(same_text(text, "void")) { return token_kind::kw_void; }
    if(same_text(text, "if")) { return token_kind::kw_if; }
    if(same_text(text, "else")) { return token_kind::kw_else; }
    if(same_text(text, "while")) { return token_kind::kw_while; }
    if(same_text(text, "return")) { return token_kind::kw_return; }
    return token_kind::identifier;
}
