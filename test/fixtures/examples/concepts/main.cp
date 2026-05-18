import std.iter;

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

    next(self: range_iter&) -> optional<i32>
    {
        if(current_value >= end_value) {
            return optional<i32>::none;
        }

        let value = current_value;
        current_value += 1;
        return optional<i32>::some(value);
    }
}

impl range {
    iter(self: range&) -> range_iter
    {
        return range_iter{ .current_value = begin, .end_value = end };
    }
}

impl iterable for range {
    type iter_type = range_iter;
    type iter_item = i32;
}

main() -> i32
{
    let total = 0;
    let values = range{ .begin = 1, .end = 5 };
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
