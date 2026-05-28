export module std.memory.span;

import std.core.iter;
import std.core.option;

export struct span<T> {
    ptr: T*;
    len: usize;
}

export concept contiguous_mutable_range {
    type item;

    data(self like&) -> item like*;
    size(self const&) -> usize;
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

impl iterable for span<T> {
    type iter_type = ptr_iter<T>;
    type iter_item = T&;

    iter(self&) -> ptr_iter<T>
    {
        return ptr_iter<T>{ .current = data(), .end = data() + len };
    }
}

impl const_iterable for span<T> {
    type const_iter_type = const_ptr_iter<T>;
    type const_iter_item = T const&;

    iter(self const&) -> const_ptr_iter<T>
    {
        return const_ptr_iter<T>{ .current = data(), .end = data() + len };
    }
}

impl<T> contiguous_mutable_range for span<T> {
    type item = T;
}

impl<T, N: usize> contiguous_mutable_range for [T; N] {
    type item = T;

    data(self like&) -> T like*
    {
        return &self[0 as usize];
    }

    size(self const&) -> usize
    {
        return N;
    }
}
