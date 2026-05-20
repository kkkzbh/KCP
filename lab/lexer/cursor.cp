export module lexer.cursor;

import std;
import source;
import lexer.token;

export struct lexer_cursor {
    source: str;
    offset: usize;
}

impl lexer_cursor {
    lexer_cursor(input: str)
    {
        return lexer_cursor{
            .source = input,
            .offset = 0 as usize
        };
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

    advance(self&, count: usize = 1 as usize) -> void
    {
        assert(offset + count <= source.size(), "lexer cursor advance past eof");
        offset += count;
    }

    slice(self const&, start: usize, end: usize) -> str
    {
        assert(start <= end and end <= source.size(), "lexer cursor slice out of bounds");
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
