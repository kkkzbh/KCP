export module preprocessor;

import std;
export import source;
export import diagnostic;

export struct preprocessed_file {
    normalized_text: string;
    diagnostics: vector<diagnostic>;
}

struct preprocessor_state {
    source: source_file const&;
    normalized_text: string;
    diagnostics: diagnostic_collector;
    offset: usize;
}

impl preprocessor_state {
    preprocessor_state(input: source_file const&)
    {
        return preprocessor_state{
            .source = input,
            .normalized_text = string{input.as_str()},
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

    blank_at(self&, index: usize) -> void
    {
        normalized_text[index] = ' ';
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
            .normalized_text = move normalized_text,
            .diagnostics = diagnostics.take()
        };
    }

    skip_line_comment(self&) -> void
    {
        blank_at(offset);
        blank_at(offset + 1);
        advance(2 as usize);
        while(not eof() and current() != '\n') {
            blank_at(offset);
            advance();
        }
    }

    skip_block_comment(self&) -> void
    {
        let start = offset;
        blank_at(offset);
        blank_at(offset + 1);
        advance(2 as usize);

        while(not eof()) {
            if(current() == '*' and peek() == '/') {
                blank_at(offset);
                blank_at(offset + 1);
                advance(2 as usize);
                return;
            }
            if(current() != '\n') {
                blank_at(offset);
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
    let state = preprocessor_state{source};
    return state.run();
}
