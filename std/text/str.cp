export module std.text.str;

import std.core.iter;
import std.core.option;

impl str {
    size(self) -> usize
    {
        return len;
    }

    data(self) -> char const*
    {
        return ptr;
    }
}

export struct str_iter {
    text: str;
    index: usize;
}

impl iterator for str_iter {
    type iter_item = char;

    next(self&) -> optional<char>
    {
        if(index >= text.size()) {
            return optional<char>::none;
        }
        let ch = text[index];
        index += 1;
        return optional<char>::some(ch);
    }
}

impl iterable for str {
    type iter_type = str_iter;
    type iter_item = char;

    iter(self&) -> str_iter
    {
        return str_iter{ .text = self, .index = 0 };
    }
}
