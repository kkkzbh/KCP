export module std.ranges.iota;

import std.iter;
import std.option;

export struct iota_iter {
    current: i32;
    end: i32;
}

impl iterator for iota_iter {
    type iter_item = i32;

    next(self&) -> optional<i32>
    {
        if(current >= end) {
            return optional<i32>::none;
        }

        let value = current;
        current += 1;
        return optional<i32>::some(value);
    }
}

export struct iota_range {
    begin: i32;
    end: i32;
}

impl iota_range {
    iter(self&) -> iota_iter
    {
        return iota_iter{ .current = begin, .end = end };
    }
}

impl iterable for iota_range {
    type iter_type = iota_iter;
    type iter_item = i32;
}

export iota(begin: i32, end: i32) -> iota_range
{
    return iota_range{ .begin = begin, .end = end };
}
