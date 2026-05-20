struct box {
    value: i32;
}

impl box {
    get(self like&) -> i32 like&
    {
        return ref value;
    }

    add(self&, amount: i32)
    {
        value += amount;
    }

    reset(self&) = delete;
    operator =(self&, rhs: this const&) = delete;
}

by_value(value: i32) -> i32
{
    return value;
}

by_ref(value: i32&) -> i32
{
    value += 1;
    return value;
}

by_const(value: i32 const&) -> i32
{
    return value;
}

take_move(value: i32 move&) -> i32
{
    return value;
}

main() -> i32
{
    let item = box{ 40 };
    item.add(1);
    let ref alias = item.get();

    const fixed = box{ 2 };
    let borrowed = by_ref(ref alias);
    let readonly = by_const(const ref alias);

    let moved_ref = 4;
    let moved_value = 5;
    return borrowed + readonly + fixed.get() + take_move(move moved_ref) + by_value(move moved_value) - 3;
}
