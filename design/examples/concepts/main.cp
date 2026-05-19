import std.option;
import std.iter;

concept sized_iterator {
    requires iterator;

    remaining(self const&) -> i32;
}

struct range {
    begin: i32;
    end: i32;
}

struct range_iter {
    current_value: i32;
    end_value: i32;
}

impl iterator for range_iter {
    type iter_item = i32;

    next(self&) -> optional<i32>
    {
        if(current_value >= end_value) {
            return optional<i32>::none;
        }

        let value = current_value;
        current_value += 1;
        return optional<i32>::some(value);
    }
}

impl sized_iterator for range_iter {
    remaining(self const&) -> i32
    {
        return end_value - current_value;
    }
}

impl range {
    iter(self&) -> range_iter
    {
        return range_iter{ .current_value = begin, .end_value = end };
    }
}

impl iterable for range {
    type iter_type = range_iter;
    type iter_item = i32;
}

sum(values: range) -> i32
{
    let total = 0;
    let iter = values.iter();

    while(true) {
        let next = iter.next();
        match next {
            .some(value) => {
                total += value;
            },
            .none => {
                break;
            },
        };
    }

    return total;
}

main() -> i32
{
    type value_type = range_iter::iter_item;
    let values = range{ .begin = value_type(1), .end = value_type(5) };
    return sum(values);
}
