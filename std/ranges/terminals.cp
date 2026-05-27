export module std.ranges.terminals;

export import std.core.iter;
export import std.core.option;
export import std.meta;

export count<I: iterator>(source: I) -> usize
{
    let iter = move source;
    let total: usize = 0;
    while(iter.next().has_value()) {
        total += 1;
    }
    return total;
}

export fold<I: iterator, Acc, F>(source: I, init: Acc, op: F) -> Acc
requires
    F: callable<Acc, I::iter_item> and call_result<F, Acc, I::iter_item> == Acc
{
    let iter = move source;
    let state = move init;
    while(true) {
        let item = iter.next();
        if(not item.has_value()) {
            return move state;
        }
        state = op(move state, *item);
    }
    unreachable();
}

export any<I: iterator, P>(source: I, predicate: P) -> bool
requires
    P: callable<I::iter_item> and call_result<P, I::iter_item> == bool
{
    let iter = move source;
    while(true) {
        let item = iter.next();
        if(not item.has_value()) {
            return false;
        }
        if(predicate(*item)) {
            return true;
        }
    }
    unreachable();
}

export all_of<I: iterator, P>(source: I, predicate: P) -> bool
requires
    P: callable<I::iter_item> and call_result<P, I::iter_item> == bool
{
    let iter = move source;
    while(true) {
        let item = iter.next();
        if(not item.has_value()) {
            return true;
        }
        if(not predicate(*item)) {
            return false;
        }
    }
    unreachable();
}

export find<I: iterator, P>(source: I, predicate: P) -> optional<I::iter_item>
requires
    P: callable<I::iter_item> and call_result<P, I::iter_item> == bool
{
    let iter = move source;
    while(true) {
        let item = iter.next();
        if(not item.has_value()) {
            return item;
        }
        if(predicate(*item)) {
            return item;
        }
    }
    unreachable();
}
