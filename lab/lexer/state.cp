export module lexer.state;

import std;
import diagnostic;
import lexer.charset;
import lexer.cursor;
import lexer.keyword;
import lexer.token;

export struct lexer {
    cursor: lexer_cursor;
    diagnostics: diagnostic_collector;
}

impl lexer {
    lexer(input: str)
    {
        return lexer{ .cursor = lexer_cursor{input}, .diagnostics = diagnostic_collector{} };
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
            tokens.push_back(item);
            if(item.kind == token_kind::eof) {
                break;
            }
        }
        return move tokens;
    }

    next(self&) -> token
    {
        skip_trivia();
        let start = cursor.offset;
        if(cursor.eof()) {
            return cursor.make_token(token_kind::eof, start, cursor.offset);
        }
        let ch = cursor.peek();
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
        while(not cursor.eof() and is_space(cursor.peek())) {
            cursor.advance();
        }
    }

    lex_identifier(self&) -> token
    {
        let start = cursor.offset;
        while(is_identifier_continue(cursor.peek())) {
            cursor.advance();
        }
        let text = cursor.slice(start, cursor.offset);
        let kind = keyword_kind(text);
        if(kind != token_kind::identifier) {
            return cursor.make_token(kind, start, cursor.offset);
        }
        return token{ .kind = kind, .span = cursor.make_span(start, cursor.offset), .text = string{text} };
    }

    lex_number(self&) -> token
    {
        let start = cursor.offset;
        let leading_zero = cursor.peek() == '0';
        cursor.advance();
        while(is_decimal_digit(cursor.peek())) {
            cursor.advance();
        }
        let has_suffix = is_identifier_start(cursor.peek());
        if(has_suffix) {
            while(is_identifier_continue(cursor.peek())) {
                cursor.advance();
            }
            diagnostics.report(diagnostic_kind::invalid_number_suffix, cursor.make_span(start, cursor.offset));
            return cursor.make_token(token_kind::invalid, start, cursor.offset);
        }
        if(leading_zero and cursor.offset > start + 1) {
            diagnostics.report(diagnostic_kind::leading_zero_integer, cursor.make_span(start, cursor.offset));
            return cursor.make_token(token_kind::invalid, start, cursor.offset);
        }
        return cursor.make_token(token_kind::integer_literal, start, cursor.offset);
    }

    lex_punctuation_or_invalid(self&) -> token
    {
        let start = cursor.offset;
        let ch = cursor.peek();
        cursor.advance();
        if(ch == '(') { return cursor.make_token(token_kind::l_paren, start, cursor.offset); }
        if(ch == ')') { return cursor.make_token(token_kind::r_paren, start, cursor.offset); }
        if(ch == '{') { return cursor.make_token(token_kind::l_brace, start, cursor.offset); }
        if(ch == '}') { return cursor.make_token(token_kind::r_brace, start, cursor.offset); }
        if(ch == ',') { return cursor.make_token(token_kind::comma, start, cursor.offset); }
        if(ch == ';') { return cursor.make_token(token_kind::semicolon, start, cursor.offset); }
        if(ch == '+') { return cursor.make_token(token_kind::plus, start, cursor.offset); }
        if(ch == '-') { return cursor.make_token(token_kind::minus, start, cursor.offset); }
        if(ch == '*') { return cursor.make_token(token_kind::star, start, cursor.offset); }
        if(ch == '/') { return cursor.make_token(token_kind::slash, start, cursor.offset); }
        if(ch == '%') { return cursor.make_token(token_kind::percent, start, cursor.offset); }
        if(ch == '=' and cursor.peek() == '=') {
            cursor.advance();
            return cursor.make_token(token_kind::equal_equal, start, cursor.offset);
        }
        if(ch == '!' and cursor.peek() == '=') {
            cursor.advance();
            return cursor.make_token(token_kind::bang_equal, start, cursor.offset);
        }
        if(ch == '<' and cursor.peek() == '=') {
            cursor.advance();
            return cursor.make_token(token_kind::less_equal, start, cursor.offset);
        }
        if(ch == '>' and cursor.peek() == '=') {
            cursor.advance();
            return cursor.make_token(token_kind::greater_equal, start, cursor.offset);
        }
        if(ch == '&' and cursor.peek() == '&') {
            cursor.advance();
            return cursor.make_token(token_kind::amp_amp, start, cursor.offset);
        }
        if(ch == '|' and cursor.peek() == '|') {
            cursor.advance();
            return cursor.make_token(token_kind::pipe_pipe, start, cursor.offset);
        }
        if(ch == '=') { return cursor.make_token(token_kind::equal, start, cursor.offset); }
        if(ch == '<') { return cursor.make_token(token_kind::less, start, cursor.offset); }
        if(ch == '>') { return cursor.make_token(token_kind::greater, start, cursor.offset); }
        diagnostics.report(diagnostic_kind::invalid_character, cursor.make_span(start, cursor.offset));
        return cursor.make_token(token_kind::invalid, start, cursor.offset);
    }
}
