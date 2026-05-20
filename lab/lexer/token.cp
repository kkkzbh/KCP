export module lexer.token;

import std;
import source;

export enum token_kind : u8 {
    eof = 0;
    invalid = 1;
    identifier = 2;
    integer_literal = 3;
    kw_int = 4;
    kw_void = 5;
    kw_if = 6;
    kw_else = 7;
    kw_while = 8;
    kw_return = 9;
    l_paren = 10;
    r_paren = 11;
    l_brace = 12;
    r_brace = 13;
    comma = 14;
    semicolon = 15;
    plus = 16;
    minus = 17;
    star = 18;
    slash = 19;
    percent = 20;
    equal = 21;
    equal_equal = 22;
    bang_equal = 23;
    less = 24;
    less_equal = 25;
    greater = 26;
    greater_equal = 27;
    amp_amp = 28;
    pipe_pipe = 29;
}

export struct token {
    kind: token_kind;
    span: source_span;
    text: string;
}

impl token {
    token()
    {
        return token{
            .kind = token_kind::invalid,
            .span = source_span{},
            .text = string{}
        };
    }
}

export token_kind_name(kind: token_kind) -> str
{
    if(kind == token_kind::eof) { return "eof"; }
    if(kind == token_kind::invalid) { return "invalid"; }
    if(kind == token_kind::identifier) { return "identifier"; }
    if(kind == token_kind::integer_literal) { return "integer_literal"; }
    if(kind == token_kind::kw_int) { return "kw_int"; }
    if(kind == token_kind::kw_void) { return "kw_void"; }
    if(kind == token_kind::kw_if) { return "kw_if"; }
    if(kind == token_kind::kw_else) { return "kw_else"; }
    if(kind == token_kind::kw_while) { return "kw_while"; }
    if(kind == token_kind::kw_return) { return "kw_return"; }
    if(kind == token_kind::l_paren) { return "l_paren"; }
    if(kind == token_kind::r_paren) { return "r_paren"; }
    if(kind == token_kind::l_brace) { return "l_brace"; }
    if(kind == token_kind::r_brace) { return "r_brace"; }
    if(kind == token_kind::comma) { return "comma"; }
    if(kind == token_kind::semicolon) { return "semicolon"; }
    if(kind == token_kind::plus) { return "plus"; }
    if(kind == token_kind::minus) { return "minus"; }
    if(kind == token_kind::star) { return "star"; }
    if(kind == token_kind::slash) { return "slash"; }
    if(kind == token_kind::percent) { return "percent"; }
    if(kind == token_kind::equal) { return "equal"; }
    if(kind == token_kind::equal_equal) { return "equal_equal"; }
    if(kind == token_kind::bang_equal) { return "bang_equal"; }
    if(kind == token_kind::less) { return "less"; }
    if(kind == token_kind::less_equal) { return "less_equal"; }
    if(kind == token_kind::greater) { return "greater"; }
    if(kind == token_kind::greater_equal) { return "greater_equal"; }
    if(kind == token_kind::amp_amp) { return "amp_amp"; }
    return "pipe_pipe";
}
