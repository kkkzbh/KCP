export module std.ranges.iota;

export import std.compare;
export import std.ranges.sources;

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

export struct iota_view<T: equality_comparable<T> and T: incrementable> {
    begin_value: T;
    end_value: T;
}

impl<T: equality_comparable<T> and T: incrementable> iterable for iota_view<T> {
    type iter_type = iota_iter<T>;
    type iter_item = T;

    iter(self&) -> iota_iter<T>
    {
        return iota_iter<T>{ .current = begin_value, .end = end_value };
    }
}

impl<T: equality_comparable<T> and T: incrementable> const_iterable for iota_view<T> {
    type const_iter_type = iota_iter<T>;
    type const_iter_item = T;

    iter(self const&) -> iota_iter<T>
    {
        return iota_iter<T>{ .current = begin_value, .end = end_value };
    }
}

impl<T: equality_comparable<T> and T: incrementable> view for iota_view<T> {
}

export iota<T: equality_comparable<T> and T: incrementable>(begin: T, end: T) -> iota_view<T>
{
    return iota_view<T>{ .begin_value = begin, .end_value = end };
}
