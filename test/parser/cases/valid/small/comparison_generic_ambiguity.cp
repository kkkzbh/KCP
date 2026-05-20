id<T>(value: T) -> T
{
    return value;
}

struct box<T> {
    value: T;
}

impl box<T> {
    make(value: T) -> box<T>
    {
        return box<T>{ value };
    }
}

main() -> i32
{
    let value = -1;
    if(value < 0) {
        let fixed = 0 as i32;
    }
    if((value < 10) and (value >= 0)) {
        let bounded = id<i32>(value);
    }
    while(value < 3) {
        value = value + 1;
    }
    let before_generic_call = value < id<i32>(4);
    let before_associated_call = value < box<i32>::make(5).value;
    let after = id<i32>(value);
    return after;
}
