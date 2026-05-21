export module std.ranges.iota;

export import std.compare;
export import std.core.iter;
export import std.core.option;

export struct iota_iter<T: equality_comparable<T> and T: incrementable> {
    current: T;
    end: T;
}

impl<T: equality_comparable<T> and T: incrementable> iterator for iota_iter<T> {
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

export struct iota_range<T: equality_comparable<T> and T: incrementable> {
    begin: T;
    end: T;
}

impl<T: equality_comparable<T> and T: incrementable> iota_range<T> {
    iter(self&) -> iota_iter<T>
    {
        return iota_iter<T>{ .current = begin, .end = end };
    }
}

impl<T: equality_comparable<T> and T: incrementable> iterable for iota_range<T> {
    type iter_type = iota_iter<T>;
    type iter_item = T;
}

export iota<T: equality_comparable<T> and T: incrementable>(begin: T, end: T) -> iota_range<T>
{
    return iota_range<T>{ .begin = begin, .end = end };
}
