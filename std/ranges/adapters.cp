export module std.ranges.adapters;

export import std.compare;
export import std.ranges.sources;

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

export struct take_view<V: iterable> {
    source: V;
    count: usize;
}

impl<V: iterable> iterable for take_view<V> {
    type iter_type = take_iter<V::iter_type>;
    type iter_item = V::iter_item;

    iter(self&) -> take_iter<V::iter_type>
    {
        return take_iter<V::iter_type>{ .inner = source.iter(), .remaining = count };
    }
}

impl<V: iterable and const_iterable> const_iterable for take_view<V> {
    type const_iter_type = take_iter<V::const_iter_type>;
    type const_iter_item = V::const_iter_item;

    iter(self const&) -> take_iter<V::const_iter_type>
    {
        return take_iter<V::const_iter_type>{ .inner = source.iter(), .remaining = count };
    }
}

impl<V: iterable> view for take_view<V> {
}

export take<R>(source: R forward&, count: usize)
{
    let view = to_view(forward source);
    type V = decltype(view);
    return take_view<V>{ .source = move view, .count = count };
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

export struct drop_view<V: iterable> {
    source: V;
    count: usize;
}

impl<V: iterable> iterable for drop_view<V> {
    type iter_type = drop_iter<V::iter_type>;
    type iter_item = V::iter_item;

    iter(self&) -> drop_iter<V::iter_type>
    {
        return drop_iter<V::iter_type>{ .inner = source.iter(), .remaining = count };
    }
}

impl<V: iterable and const_iterable> const_iterable for drop_view<V> {
    type const_iter_type = drop_iter<V::const_iter_type>;
    type const_iter_item = V::const_iter_item;

    iter(self const&) -> drop_iter<V::const_iter_type>
    {
        return drop_iter<V::const_iter_type>{ .inner = source.iter(), .remaining = count };
    }
}

impl<V: iterable> view for drop_view<V> {
}

export drop<R>(source: R forward&, count: usize)
{
    let view = to_view(forward source);
    type V = decltype(view);
    return drop_view<V>{ .source = move view, .count = count };
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

export struct filter_view<V: iterable, P> {
    source: V;
    predicate: P;
}

impl<V: iterable, P> iterable for filter_view<V, P>
requires
    P: callable<V::iter_item> and call_result<P, V::iter_item> == bool
{
    type iter_type = filter_iter<V::iter_type, P>;
    type iter_item = V::iter_item;

    iter(self&) -> filter_iter<V::iter_type, P>
    {
        return filter_iter<V::iter_type, P>{ .inner = source.iter(), .predicate = predicate };
    }
}

impl<V: iterable and const_iterable, P> const_iterable for filter_view<V, P>
requires
    P: callable<V::const_iter_item> and call_result<P, V::const_iter_item> == bool
{
    type const_iter_type = filter_iter<V::const_iter_type, P>;
    type const_iter_item = V::const_iter_item;

    iter(self const&) -> filter_iter<V::const_iter_type, P>
    {
        return filter_iter<V::const_iter_type, P>{ .inner = source.iter(), .predicate = predicate };
    }
}

impl<V: iterable, P> view for filter_view<V, P>
requires
    P: callable<V::iter_item> and call_result<P, V::iter_item> == bool
{
}

export filter<R, P>(source: R forward&, predicate: P)
{
    let view = to_view(forward source);
    type V = decltype(view);
    return filter_view<V, P>{ .source = move view, .predicate = move predicate };
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

export struct transform_view<V: iterable, F> {
    source: V;
    mapper: F;
}

impl<V: iterable, F> iterable for transform_view<V, F>
requires
    F: callable<V::iter_item>
{
    type iter_type = transform_iter<V::iter_type, F>;
    type iter_item = call_result<F, V::iter_item>;

    iter(self&) -> transform_iter<V::iter_type, F>
    {
        return transform_iter<V::iter_type, F>{ .inner = source.iter(), .mapper = mapper };
    }
}

impl<V: iterable and const_iterable, F> const_iterable for transform_view<V, F>
requires
    F: callable<V::const_iter_item>
{
    type const_iter_type = transform_iter<V::const_iter_type, F>;
    type const_iter_item = call_result<F, V::const_iter_item>;

    iter(self const&) -> transform_iter<V::const_iter_type, F>
    {
        return transform_iter<V::const_iter_type, F>{ .inner = source.iter(), .mapper = mapper };
    }
}

impl<V: iterable, F> view for transform_view<V, F>
requires
    F: callable<V::iter_item>
{
}

export transform<R, F>(source: R forward&, mapper: F)
{
    let view = to_view(forward source);
    type V = decltype(view);
    return transform_view<V, F>{ .source = move view, .mapper = move mapper };
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

export struct enumerate_view<V: iterable> {
    source: V;
}

impl<V: iterable> iterable for enumerate_view<V> {
    type iter_type = enumerate_iter<V::iter_type>;
    type iter_item = (usize, V::iter_item);

    iter(self&) -> enumerate_iter<V::iter_type>
    {
        return enumerate_iter<V::iter_type>{ .inner = source.iter(), .index = 0 as usize };
    }
}

impl<V: iterable and const_iterable> const_iterable for enumerate_view<V> {
    type const_iter_type = enumerate_iter<V::const_iter_type>;
    type const_iter_item = (usize, V::const_iter_item);

    iter(self const&) -> enumerate_iter<V::const_iter_type>
    {
        return enumerate_iter<V::const_iter_type>{ .inner = source.iter(), .index = 0 as usize };
    }
}

impl<V: iterable> view for enumerate_view<V> {
}

export enumerate<R>(source: R forward&)
{
    let view = to_view(forward source);
    type V = decltype(view);
    return enumerate_view<V>{ .source = move view };
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

export struct zip_view<Left: iterable, Right: iterable> {
    left: Left;
    right: Right;
}

impl<Left: iterable, Right: iterable> iterable for zip_view<Left, Right> {
    type iter_type = zip_iter<Left::iter_type, Right::iter_type>;
    type iter_item = (Left::iter_item, Right::iter_item);

    iter(self&) -> zip_iter<Left::iter_type, Right::iter_type>
    {
        return zip_iter<Left::iter_type, Right::iter_type>{ .left = left.iter(), .right = right.iter() };
    }
}

impl<Left: iterable and const_iterable, Right: iterable and const_iterable> const_iterable for zip_view<Left, Right> {
    type const_iter_type = zip_iter<Left::const_iter_type, Right::const_iter_type>;
    type const_iter_item = (Left::const_iter_item, Right::const_iter_item);

    iter(self const&) -> zip_iter<Left::const_iter_type, Right::const_iter_type>
    {
        return zip_iter<Left::const_iter_type, Right::const_iter_type>{ .left = left.iter(), .right = right.iter() };
    }
}

impl<Left: iterable, Right: iterable> view for zip_view<Left, Right> {
}

export zip<Left, Right>(left: Left forward&, right: Right forward&)
{
    let left_view = to_view(forward left);
    let right_view = to_view(forward right);
    type LeftView = decltype(left_view);
    type RightView = decltype(right_view);
    return zip_view<LeftView, RightView>{ .left = move left_view, .right = move right_view };
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

export struct concat_view<Left: iterable, Right: iterable> {
    left: Left;
    right: Right;
}

impl<Left: iterable, Right: iterable> iterable for concat_view<Left, Right>
requires
    Left::iter_item == Right::iter_item
{
    type iter_type = concat_iter<Left::iter_type, Right::iter_type>;
    type iter_item = Left::iter_item;

    iter(self&) -> concat_iter<Left::iter_type, Right::iter_type>
    {
        return concat_iter<Left::iter_type, Right::iter_type>{ .left = left.iter(), .right = right.iter(), .left_done = false };
    }
}

impl<Left: iterable and const_iterable, Right: iterable and const_iterable> const_iterable for concat_view<Left, Right>
requires
    Left::const_iter_item == Right::const_iter_item
{
    type const_iter_type = concat_iter<Left::const_iter_type, Right::const_iter_type>;
    type const_iter_item = Left::const_iter_item;

    iter(self const&) -> concat_iter<Left::const_iter_type, Right::const_iter_type>
    {
        return concat_iter<Left::const_iter_type, Right::const_iter_type>{ .left = left.iter(), .right = right.iter(), .left_done = false };
    }
}

impl<Left: iterable, Right: iterable> view for concat_view<Left, Right>
requires
    Left::iter_item == Right::iter_item
{
}

export concat<Left, Right>(left: Left forward&, right: Right forward&)
{
    let left_view = to_view(forward left);
    let right_view = to_view(forward right);
    type LeftView = decltype(left_view);
    type RightView = decltype(right_view);
    return concat_view<LeftView, RightView>{ .left = move left_view, .right = move right_view };
}
