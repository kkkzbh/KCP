concept measurable {
    size(self: Self const&) -> i32;
}

struct box<T> {
    value: T;
}

impl box<T> {
    get(self: box<T> const&) -> T
    {
        return value;
    }

    replace<U>(self: box<T> const&, next: U) -> box<U>
    {
        return box<U>{ .value = next };
    }
}

impl measurable for box<T> {
    size(self: box<T> const&) -> i32
    {
        return 1;
    }
}

measure<T: measurable>(value: T) -> i32
{
    return value.size();
}

sum<T...>(values: T...) -> i32
{
    let total = 0;
    template for(let value : values...) {
        total += value;
    }
    return total;
}

type_count<T...>() -> i32
{
    let total = 0;
    template for(type U : T...) {
        type current = U;
        total += 1;
    }
    return total;
}

main() -> i32
{
    let value = box<i32>{ .value = 1 };
    let changed = value.replace<bool>(true);
    if(changed.get()) {
        return sum(10, 20, 9) + measure(value) + type_count<i32, bool>();
    }
    return 0;
}
