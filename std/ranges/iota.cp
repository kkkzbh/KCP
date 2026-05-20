export module std.ranges.iota;

import std.core.iter;
import std.core.option;

export struct iota_iter<T> {
    current: T;
    end: T;
}

impl iterator for iota_iter<T> {
    type iter_item = T;

    next(self&) -> optional<T>
    {
        if(current == end) {
            return optional<T>::none;
        }

        let value = current;
        ++current;
        return optional<T>::some(value);
    }
}

export struct iota_range<T> {
    begin: T;
    end: T;
}

impl iota_range<T> {
    iter(self&) -> iota_iter<T>
    {
        return iota_iter<T>{ .current = begin, .end = end };
    }
}

impl iterable for iota_range<T> {
    type iter_type = iota_iter<T>;
    type iter_item = T;
}

export iota<T>(begin: T, end: T) -> iota_range<T>
{
    return iota_range<T>{ .begin = begin, .end = end };
}
