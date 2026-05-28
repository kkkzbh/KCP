export module std.ranges.sources;

export import std.core.iter;
export import std.core.option;
export import std.meta;

export concept view {
    requires iterable;
}

export struct ref_view<R: iterable> {
    source: R*;
}

impl<R: iterable> iterable for ref_view<R> {
    type iter_type = R::iter_type;
    type iter_item = R::iter_item;

    iter(self&) -> R::iter_type
    {
        return (*source).iter();
    }
}

impl<R: iterable> const_iterable for ref_view<R> {
    type const_iter_type = R::iter_type;
    type const_iter_item = R::iter_item;

    iter(self const&) -> R::iter_type
    {
        return (*source).iter();
    }
}

impl<R: iterable> view for ref_view<R> {
}

export struct const_ref_view<R: const_iterable> {
    source: R const*;
}

impl<R: const_iterable> iterable for const_ref_view<R> {
    type iter_type = R::const_iter_type;
    type iter_item = R::const_iter_item;

    iter(self&) -> R::const_iter_type
    {
        return (*source).iter();
    }
}

impl<R: const_iterable> const_iterable for const_ref_view<R> {
    type const_iter_type = R::const_iter_type;
    type const_iter_item = R::const_iter_item;

    iter(self const&) -> R::const_iter_type
    {
        return (*source).iter();
    }
}

impl<R: const_iterable> view for const_ref_view<R> {
}

export struct owning_view<R: iterable> {
    source: R;
}

impl<R: iterable> iterable for owning_view<R> {
    type iter_type = R::iter_type;
    type iter_item = R::iter_item;

    iter(self&) -> R::iter_type
    {
        return source.iter();
    }
}

impl<R: iterable and const_iterable> const_iterable for owning_view<R> {
    type const_iter_type = R::const_iter_type;
    type const_iter_item = R::const_iter_item;

    iter(self const&) -> R::const_iter_type
    {
        return source.iter();
    }
}

impl<R: iterable> view for owning_view<R> {
}

export to_view<R>(source: R forward&)
{
    type source_type = decltype(forward source);
    template if(source_type: is_const_lvalue_reference) {
        return const_ref_view<R>{ .source = &source };
    } else template if(source_type: is_lvalue_reference) {
        return ref_view<R>{ .source = &source };
    } else {
        return owning_view<R>{ .source = forward source };
    }
}

export struct empty_iter<T> {
}

impl<T> iterator for empty_iter<T> {
    type iter_item = T;

    next(self&) -> optional<T>
    {
        return optional<T>::none;
    }
}

export struct empty_view<T> {
}

impl<T> iterable for empty_view<T> {
    type iter_type = empty_iter<T>;
    type iter_item = T;

    iter(self&) -> empty_iter<T>
    {
        return empty_iter<T>{};
    }
}

impl<T> const_iterable for empty_view<T> {
    type const_iter_type = empty_iter<T>;
    type const_iter_item = T;

    iter(self const&) -> empty_iter<T>
    {
        return empty_iter<T>{};
    }
}

impl<T> view for empty_view<T> {
}

export empty<T>() -> empty_view<T>
{
    return empty_view<T>{};
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

export struct single_view<T> {
    value: T;
}

impl<T> iterable for single_view<T> {
    type iter_type = single_iter<T>;
    type iter_item = T;

    iter(self&) -> single_iter<T>
    {
        return single_iter<T>{ .value = value, .remaining = true };
    }
}

impl<T> const_iterable for single_view<T> {
    type const_iter_type = single_iter<T>;
    type const_iter_item = T;

    iter(self const&) -> single_iter<T>
    {
        return single_iter<T>{ .value = value, .remaining = true };
    }
}

impl<T> view for single_view<T> {
}

export single<T>(value: T) -> single_view<T>
{
    return single_view<T>{ .value = move value };
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

export struct repeat_view<T> {
    value: T;
}

impl<T> iterable for repeat_view<T> {
    type iter_type = repeat_iter<T>;
    type iter_item = T;

    iter(self&) -> repeat_iter<T>
    {
        return repeat_iter<T>{ .value = value };
    }
}

impl<T> const_iterable for repeat_view<T> {
    type const_iter_type = repeat_iter<T>;
    type const_iter_item = T;

    iter(self const&) -> repeat_iter<T>
    {
        return repeat_iter<T>{ .value = value };
    }
}

impl<T> view for repeat_view<T> {
}

export repeat<T>(value: T) -> repeat_view<T>
{
    return repeat_view<T>{ .value = move value };
}

impl<T, N: usize> iterable for [T; N] {
    type iter_type = ptr_iter<T>;
    type iter_item = T&;

    iter(self&) -> ptr_iter<T>
    {
        return ptr_iter<T>{ .current = &self[0 as usize], .end = &self[0 as usize] + N };
    }
}

impl<T, N: usize> const_iterable for [T; N] {
    type const_iter_type = const_ptr_iter<T>;
    type const_iter_item = T const&;

    iter(self const&) -> const_ptr_iter<T>
    {
        return const_ptr_iter<T>{ .current = &self[0 as usize], .end = &self[0 as usize] + N };
    }
}
