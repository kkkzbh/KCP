export module std.memory.span;

import std.core.iter;
import std.core.option;

export struct span<T> {
    ptr: T*;
    len: usize;
}

export struct span_iter<T> {
    current: T*;
    end: T*;
}

impl span<T> {
    span(ptr: T*, len: usize)
    {
        return span<T>{ .ptr = ptr, .len = len };
    }

    data(self like&) -> T like*
    {
        return ptr;
    }

    size(self const&) -> usize
    {
        return len;
    }

    empty(self const&) -> bool
    {
        return len == 0;
    }

    operator [](self like&, index: usize) -> T like&
    {
        assert(index < len, "span index out of bounds");
        return ref ptr[index];
    }
}

impl iterator for span_iter<T> {
    type iter_item = T&;

    next(self&) -> optional<T&>
    {
        if(current >= end) {
            return optional<T&>::none;
        }

        let item = current;
        current = current + 1;
        return optional<T&>::some(ref *item);
    }
}

impl iterable for span<T> {
    type iter_type = span_iter<T>;
    type iter_item = T&;

    iter(self&) -> span_iter<T>
    {
        return span_iter<T>{ .current = ptr, .end = ptr + len };
    }
}
