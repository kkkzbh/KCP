export module std.span;

import std.iter;
import std.option;
import std.detail.runtime;

export struct span<T> {
    ptr: T const*;
    len: usize;
}

export struct span_iter<T> {
    current: T const*;
    end: T const*;
}

impl span<T> {
    span(ptr: T const*, len: usize)
    {
        return span<T>{ .ptr = ptr, .len = len };
    }

    data(self const&) -> T const*
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

    operator [](self const&, index: usize) -> T const&
    {
        if(index >= len) {
            cp_bounds_fail();
        }

        return const ref ptr[index];
    }
}

impl iterator for span_iter<T> {
    type iter_item = T const&;

    next(self&) -> optional<T const&>
    {
        if(current >= end) {
            return optional<T const&>::none;
        }

        let item = current;
        current = current + 1;
        return optional<T const&>::some(const ref *item);
    }
}

impl iterable for span<T> {
    type iter_type = span_iter<T>;
    type iter_item = T const&;

    iter(self&) -> span_iter<T>
    {
        return span_iter<T>{ .current = ptr, .end = ptr + len };
    }
}
