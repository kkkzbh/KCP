export module preprocessor;

import std;
import source;
import diagnostic;

export struct preprocessed_file {
    text: string;
    diagnostics: vector<diagnostic>;
}

struct preprocessor {
    source: source_file const&;
    text: string;
    diagnostics: diagnostic_collector;
    offset: usize;
}

impl preprocessor {
    preprocessor(input: source_file const&)
    {
        return preprocessor{
            .source = input,
            .text = string{input.as_str()},
            .diagnostics = diagnostic_collector{},
            .offset = 0 as usize
        };
    }

    eof(self const&) -> bool
    {
        return offset >= source.size();
    }

    current(self const&) -> char
    {
        return source.as_str()[offset];
    }

    peek(self const&, distance: usize = 1 as usize) -> char
    {
        let index = offset + distance;
        if(index >= source.size()) {
            return '\0';
        }
        return source.as_str()[index];
    }

    advance(self&, count: usize = 1 as usize) -> void
    {
        offset += count;
    }

    run(self&) -> preprocessed_file
    {
        while(not eof()) {
            if(current() == '/' and peek() == '/') {
                skip_line_comment();
            } else if(current() == '/' and peek() == '*') {
                skip_block_comment();
            } else {
                advance();
            }
        }

        return preprocessed_file{
            .text = move text,
            .diagnostics = diagnostics.take()
        };
    }

    skip_line_comment(self&) -> void
    {
        text[offset] = ' ';
        text[offset + 1] = ' ';
        advance(2 as usize);
        while(not eof() and current() != '\n') {
            text[offset] = ' ';
            advance();
        }
    }

    skip_block_comment(self&) -> void
    {
        let start = offset;
        text[offset] = ' ';
        text[offset + 1] = ' ';
        advance(2 as usize);

        while(not eof()) {
            if(current() == '*' and peek() == '/') {
                text[offset] = ' ';
                text[offset + 1] = ' ';
                advance(2 as usize);
                return;
            }
            if(current() != '\n') {
                text[offset] = ' ';
            }
            advance();
        }

        diagnostics.report(
            diagnostic_kind::unterminated_block_comment,
            source_span{ .start = start, .end = source.size() }
        );
    }
}

export preprocess(source: source_file const&) -> preprocessed_file
{
    let state = preprocessor{source};
    return state.run();
}
