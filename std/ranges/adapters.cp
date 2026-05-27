export module std.ranges.adapters;

export import std.core.iter;
export import std.core.option;
export import std.meta;

export struct take_iter<I: iterator> {
    inner: I;
    remaining: usize;
}

impl<I: iterator> iterator for take_iter<I> {
    type iter_item = I::iter_item;

    next(self&) -> optional<I::iter_item>
    {
        if(remaining == 0) {
            return optional<I::iter_item>::none;
        }

        let item = inner.next();
        if(item.has_value()) {
            remaining -= 1;
        }
        return item;
    }
}

export take<I: iterator>(source: I, count: usize) -> take_iter<I>
{
    return take_iter<I>{ .inner = move source, .remaining = count };
}

export struct drop_iter<I: iterator> {
    inner: I;
    remaining: usize;
}

impl<I: iterator> iterator for drop_iter<I> {
    type iter_item = I::iter_item;

    next(self&) -> optional<I::iter_item>
    {
        while(remaining > 0) {
            let skipped = inner.next();
            if(not skipped.has_value()) {
                return optional<I::iter_item>::none;
            }
            remaining -= 1;
        }
        return inner.next();
    }
}

export drop<I: iterator>(source: I, count: usize) -> drop_iter<I>
{
    return drop_iter<I>{ .inner = move source, .remaining = count };
}

export struct filter_iter<I: iterator, P> {
    inner: I;
    predicate: P;
}

impl<I: iterator, P> iterator for filter_iter<I, P>
requires
    P: callable<I::iter_item> and call_result<P, I::iter_item> == bool
{
    type iter_item = I::iter_item;

    next(self&) -> optional<I::iter_item>
    {
        while(true) {
            let item = inner.next();
            if(not item.has_value()) {
                return item;
            }
            if(predicate(*item)) {
                return item;
            }
        }
        unreachable();
    }
}

export filter<I: iterator, P>(source: I, predicate: P) -> filter_iter<I, P>
requires
    P: callable<I::iter_item> and call_result<P, I::iter_item> == bool
{
    return filter_iter<I, P>{ .inner = move source, .predicate = move predicate };
}

export struct transform_iter<I: iterator, F> {
    inner: I;
    mapper: F;
}

impl<I: iterator, F> iterator for transform_iter<I, F>
requires
    F: callable<I::iter_item>
{
    type iter_item = call_result<F, I::iter_item>;

    next(self&) -> optional<this::iter_item>
    {
        let item = inner.next();
        if(not item.has_value()) {
            return optional<this::iter_item>::none;
        }
        return optional<this::iter_item>::some(mapper(*item));
    }
}

export transform<I: iterator, F>(source: I, mapper: F) -> transform_iter<I, F>
requires
    F: callable<I::iter_item>
{
    return transform_iter<I, F>{ .inner = move source, .mapper = move mapper };
}

export struct enumerate_iter<I: iterator> {
    inner: I;
    index: usize;
}

impl<I: iterator> iterator for enumerate_iter<I> {
    type iter_item = (usize, I::iter_item);

    next(self&) -> optional<this::iter_item>
    {
        let item = inner.next();
        if(not item.has_value()) {
            return optional<this::iter_item>::none;
        }

        let current = index;
        index += 1;
        return optional<this::iter_item>::some((current, *item));
    }
}

export enumerate<I: iterator>(source: I) -> enumerate_iter<I>
{
    return enumerate_iter<I>{ .inner = move source, .index = 0 };
}

export struct zip_iter<Left: iterator, Right: iterator> {
    left: Left;
    right: Right;
}

impl<Left: iterator, Right: iterator> iterator for zip_iter<Left, Right> {
    type iter_item = (Left::iter_item, Right::iter_item);

    next(self&) -> optional<this::iter_item>
    {
        let left_item = left.next();
        if(not left_item.has_value()) {
            return optional<this::iter_item>::none;
        }

        let right_item = right.next();
        if(not right_item.has_value()) {
            return optional<this::iter_item>::none;
        }

        return optional<this::iter_item>::some((*left_item, *right_item));
    }
}

export zip<Left: iterator, Right: iterator>(left: Left, right: Right) -> zip_iter<Left, Right>
{
    return zip_iter<Left, Right>{ .left = move left, .right = move right };
}

export struct concat_iter<Left: iterator, Right: iterator> {
    left: Left;
    right: Right;
    left_done: bool;
}

impl<Left: iterator, Right: iterator> iterator for concat_iter<Left, Right>
requires
    Left::iter_item == Right::iter_item
{
    type iter_item = Left::iter_item;

    next(self&) -> optional<this::iter_item>
    {
        if(not left_done) {
            let left_item = left.next();
            if(left_item.has_value()) {
                return left_item;
            }
            left_done = true;
        }
        return right.next();
    }
}

export concat<Left: iterator, Right: iterator>(left: Left, right: Right) -> concat_iter<Left, Right>
requires
    Left::iter_item == Right::iter_item
{
    return concat_iter<Left, Right>{ .left = move left, .right = move right, .left_done = false };
}
