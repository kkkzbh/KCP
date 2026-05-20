export module lexer.state;

import std;
import diagnostic;
import lexer.charset;
import lexer.keyword;
import lexer.token;
import preprocessor;
import source;

single_punctuation_kind(ch: char) -> token_kind
{
    if(ch == '(') { return token_kind::l_paren; }
    if(ch == ')') { return token_kind::r_paren; }
    if(ch == '{') { return token_kind::l_brace; }
    if(ch == '}') { return token_kind::r_brace; }
    if(ch == ',') { return token_kind::comma; }
    if(ch == ';') { return token_kind::semicolon; }
    if(ch == '+') { return token_kind::plus; }
    if(ch == '-') { return token_kind::minus; }
    if(ch == '*') { return token_kind::star; }
    if(ch == '/') { return token_kind::slash; }
    if(ch == '%') { return token_kind::percent; }
    if(ch == '=') { return token_kind::equal; }
    if(ch == '<') { return token_kind::less; }
    if(ch == '>') { return token_kind::greater; }
    return token_kind::invalid;
}

export struct lexer {
    source: str;
    offset: usize;
    diagnostics: diagnostic_collector;
}

impl lexer {
    lexer(input: preprocessed_file const&)
    {
        return lexer{ .source = input.text.as_str(), .offset = 0 as usize, .diagnostics = diagnostic_collector{} };
    }

    take_diagnostics(self&) -> vector<diagnostic>
    {
        return diagnostics.take();
    }

    tokenize_all(self&) -> vector<token>
    {
        let tokens = vector<token>{};
        while(true) {
            let item = next();
            let finished = item.kind == token_kind::eof;
            tokens.move_back(move item);
            if(finished) {
                break;
            }
        }
        return tokens;
    }

    next(self&) -> token
    {
        skip_trivia();
        let start = offset;
        if(eof()) {
            return make_token(token_kind::eof, start, offset);
        }
        let ch = peek();
        if(is_identifier_start(ch)) {
            return lex_identifier();
        }
        if(is_decimal_digit(ch)) {
            return lex_number();
        }
        return lex_punctuation_or_invalid();
    }

    skip_trivia(self&) -> void
    {
        while(not eof() and is_space(peek())) {
            advance();
        }
    }

    lex_identifier(self&) -> token
    {
        let start = offset;
        while(is_identifier_continue(peek())) {
            advance();
        }
        let text = slice(start, offset);
        let kind = keyword_kind(text);
        if(kind != token_kind::identifier) {
            return make_token(kind, start, offset);
        }
        return token{ .kind = kind, .span = make_span(start, offset), .text = string{text} };
    }

    lex_number(self&) -> token
    {
        let start = offset;
        let leading_zero = peek() == '0';
        advance();
        while(is_decimal_digit(peek())) {
            advance();
        }
        if(is_identifier_start(peek())) {
            while(is_identifier_continue(peek())) {
                advance();
            }
            diagnostics.report(diagnostic_kind::invalid_number_suffix, make_span(start, offset));
            return make_token(token_kind::invalid, start, offset);
        }
        if(leading_zero and offset > start + 1) {
            diagnostics.report(diagnostic_kind::leading_zero_integer, make_span(start, offset));
            return make_token(token_kind::invalid, start, offset);
        }
        return make_token(token_kind::integer_literal, start, offset);
    }

    lex_punctuation_or_invalid(self&) -> token
    {
        let start = offset;
        let ch = peek();
        advance();
        if(ch == '=' and consume('=')) {
            return make_token(token_kind::equal_equal, start, offset);
        }
        if(ch == '!' and consume('=')) {
            return make_token(token_kind::bang_equal, start, offset);
        }
        if(ch == '<' and consume('=')) {
            return make_token(token_kind::less_equal, start, offset);
        }
        if(ch == '>' and consume('=')) {
            return make_token(token_kind::greater_equal, start, offset);
        }
        if(ch == '&' and consume('&')) {
            return make_token(token_kind::amp_amp, start, offset);
        }
        if(ch == '|' and consume('|')) {
            return make_token(token_kind::pipe_pipe, start, offset);
        }
        let kind = single_punctuation_kind(ch);
        if(kind != token_kind::invalid) {
            return make_token(kind, start, offset);
        }
        diagnostics.report(diagnostic_kind::invalid_character, make_span(start, offset));
        return make_token(token_kind::invalid, start, offset);
    }

    eof(self const&) -> bool
    {
        return offset >= source.size();
    }

    peek(self const&, distance: usize = 0 as usize) -> char
    {
        let index = offset + distance;
        if(index >= source.size()) {
            return '\0';
        }
        return source[index];
    }

    consume(self&, expected: char) -> bool
    {
        if(peek() != expected) {
            return false;
        }
        advance();
        return true;
    }

    advance(self&, count: usize = 1 as usize) -> void
    {
        assert(offset + count <= source.size(), "lexer advance past eof");
        offset += count;
    }

    slice(self const&, start: usize, end: usize) -> str
    {
        assert(start <= end and end <= source.size(), "lexer slice out of bounds");
        return str{ .ptr = source.data() + start, .len = end - start };
    }

    make_span(self const&, start: usize, end: usize) -> source_span
    {
        return source_span{ .start = start, .end = end };
    }

    make_token(self const&, kind: token_kind, start: usize, end: usize) -> token
    {
        return token{
            .kind = kind,
            .span = make_span(start, end),
            .text = string{}
        };
    }
}
