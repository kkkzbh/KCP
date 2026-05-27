export module std.ranges.sources;

export import std.core.iter;
export import std.core.option;

export struct empty_iter<T> {
}

impl<T> iterator for empty_iter<T> {
    type iter_item = T;

    next(self&) -> optional<T>
    {
        return optional<T>::none;
    }
}

export empty<T>() -> empty_iter<T>
{
    return empty_iter<T>{};
}

export struct single_iter<T> {
    value: T;
    remaining: bool;
}

impl<T> iterator for single_iter<T> {
    type iter_item = T;

    next(self&) -> optional<T>
    {
        if(not remaining) {
            return optional<T>::none;
        }

        remaining = false;
        return optional<T>::some(move value);
    }
}

export single<T>(value: T) -> single_iter<T>
{
    return single_iter<T>{ .value = move value, .remaining = true };
}

export struct repeat_iter<T> {
    value: T;
}

impl<T> iterator for repeat_iter<T> {
    type iter_item = T;

    next(self&) -> optional<T>
    {
        return optional<T>::some(value);
    }
}

export repeat<T>(value: T) -> repeat_iter<T>
{
    return repeat_iter<T>{ .value = move value };
}

export all<R: iterable>(source: R&) -> R::iter_type
{
    return source.iter();
}

export struct array_iter<T, N: usize> {
    current: T*;
    end: T*;
}

impl<T, N: usize> iterator for array_iter<T, N> {
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

impl<T, N: usize> iterable for [T; N] {
    type iter_type = array_iter<T, N>;
    type iter_item = T&;

    iter(self&) -> array_iter<T, N>
    {
        return array_iter<T, N>{ .current = &self[0 as usize], .end = &self[0 as usize] + N };
    }
}
