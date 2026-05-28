export module std.core.iter;

import std.core.option;

export concept iterator {
    type iter_item;

    next(self&) -> optional<iter_item>;
}

export concept iterable {
    type iter_type;
    type iter_item;

    requires (
        iter_type: iterator
        and iter_type::iter_item == iter_item
    );

    iter(self&) -> iter_type;
}

export concept const_iterable {
    type const_iter_type;
    type const_iter_item;

    requires (
        const_iter_type: iterator
        and const_iter_type::iter_item == const_iter_item
    );

    iter(self const&) -> const_iter_type;
}

export struct ptr_iter<T> {
    current: T*;
    end: T*;
}

export struct const_ptr_iter<T> {
    current: T const*;
    end: T const*;
}

impl<T> iterator for ptr_iter<T> {
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

impl<T> iterator for const_ptr_iter<T> {
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
