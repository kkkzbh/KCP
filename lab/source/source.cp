export module source;

import std;

export struct source_span {
    start: usize;
    end: usize;
}

export struct source_position {
    offset: usize;
    line: usize;
    column: usize;
}

export struct source_file {
    name: string;
    text: string;
    line_starts: vector<usize>;
}

impl source_file {
    source_file() = default;

    source_file(name_value: str, text_value: str)
    {
        let result = source_file{
            .name = string{name_value},
            .text = string{text_value},
            .line_starts = vector<usize>{}
        };

        result.line_starts.push_back(0 as usize);
        for(let index : iota(0 as usize, result.text.size())) {
            if(result.text[index] == '\n') {
                result.line_starts.push_back(index + 1);
            }
        }
        return result;
    }

    size(self const&) -> usize
    {
        return text.size();
    }

    as_str(self const&) -> str
    {
        return text.as_str();
    }

    slice(self const&, span: source_span) -> str
    {
        assert(span.start <= span.end and span.end <= text.size(), "source slice out of bounds");
        return str{ .ptr = text.data() + span.start, .len = span.end - span.start };
    }

    position(self const&, offset: usize) -> source_position
    {
        assert(offset <= text.size(), "source position out of bounds");
        let line_index: usize = 0;
        while(line_index + 1 < line_starts.size() and line_starts[line_index + 1] <= offset) {
            ++line_index;
        }
        let line_start = line_starts[line_index];
        return source_position{
            .offset = offset,
            .line = line_index + 1,
            .column = offset - line_start + 1
        };
    }
}
