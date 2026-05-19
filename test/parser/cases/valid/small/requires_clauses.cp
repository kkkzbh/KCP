concept marker {
}

struct value {
    item: i32;
}

impl value
requires value: marker
{
    get(self const&) -> i32
    requires value: marker
    {
        return item;
    }
}

impl marker for value
requires value == value
{
}

main() -> i32
requires value: marker
{
    let item = value{ .item = 42 };
    return item.get();
}
