export module std.ranges.terminals;

export import std.ranges.sources;

export count<R>(source: R forward&) -> usize
{
    let view = to_view(forward source);
    let iter = view.iter();
    let total: usize = 0;
    while(iter.next().has_value()) {
        total += 1;
    }
    return total;
}

export fold<R, Acc, F>(source: R forward&, init: Acc, op: F) -> Acc
{
    let view = to_view(forward source);
    let iter = view.iter();
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

export any<R, P>(source: R forward&, predicate: P) -> bool
{
    let view = to_view(forward source);
    let iter = view.iter();
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

export all_of<R, P>(source: R forward&, predicate: P) -> bool
{
    let view = to_view(forward source);
    let iter = view.iter();
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

export find<R, P>(source: R forward&, predicate: P)
{
    let view = to_view(forward source);
    let iter = view.iter();
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
