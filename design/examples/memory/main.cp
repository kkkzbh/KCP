struct guard {
    total: i32*;
    value: i32;
}

impl guard {
    ~guard()
    {
        *total += value;
    }
}

main() -> i32
{
    let total = 0;
    let items = alloc<guard>(2);
    construct_at(items, guard{ .total = &total, .value = 10 });
    construct_at(items + 1, guard{ .total = &total, .value = 32 });
    destroy_at(items + 1);
    destroy_at(items);
    free(items);
    return total;
}
