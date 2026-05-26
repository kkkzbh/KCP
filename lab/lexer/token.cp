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
    kw_for = 10;
    kw_break = 11;
    kw_continue = 12;
    l_paren = 13;
    r_paren = 14;
    l_brace = 15;
    r_brace = 16;
    l_bracket = 17;
    r_bracket = 18;
    comma = 19;
    semicolon = 20;
    plus = 21;
    minus = 22;
    star = 23;
    slash = 24;
    percent = 25;
    equal = 26;
    equal_equal = 27;
    bang_equal = 28;
    less = 29;
    less_equal = 30;
    greater = 31;
    greater_equal = 32;
    amp_amp = 33;
    pipe_pipe = 34;
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
    if(kind == token_kind::kw_for) { return "kw_for"; }
    if(kind == token_kind::kw_break) { return "kw_break"; }
    if(kind == token_kind::kw_continue) { return "kw_continue"; }
    if(kind == token_kind::l_paren) { return "l_paren"; }
    if(kind == token_kind::r_paren) { return "r_paren"; }
    if(kind == token_kind::l_brace) { return "l_brace"; }
    if(kind == token_kind::r_brace) { return "r_brace"; }
    if(kind == token_kind::l_bracket) { return "l_bracket"; }
    if(kind == token_kind::r_bracket) { return "r_bracket"; }
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
