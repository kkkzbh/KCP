export concept iterator {
    type item;
    next(self&) -> item;
}

struct range_iter {
    value: i32;
}

impl iterator for range_iter {
    type item = i32;

    next(self&) -> i32
    {
        return value;
    }
}

main() -> i32
{
    type value_type = range_iter::item;
    let value: value_type = 1;
    let next = optional<i32>::some(value);
    let none = optional<i32>::none;
    return value;
}
