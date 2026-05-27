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

concept type_equalities {
    requires (
        [i32; 2] == [i32; 2]
        and (i32, bool) == (i32, bool)
        and f(i32) -> bool == f(value: i32) -> bool
        and decltype(1 + 1) == i32
        and storage [i32; 2] == storage [i32; 2]
    );
}

main() -> i32
requires value: marker
{
    let item = value{ .item = 42 };
    return item.get();
}
