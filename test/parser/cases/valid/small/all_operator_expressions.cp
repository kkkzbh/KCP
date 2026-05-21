id<T>(value: T) -> T
{
    return value;
}

main()
{
    let value = 16;
    let pointer: i32* = &value;
    let bool_or = true or id<bool>(false);
    let bool_and = true and id<bool>(true);
    let bit_or = value | id<i32>(1);
    let bit_xor = value ^ id<i32>(2);
    let bit_and = value & id<i32>(3);
    let equal = value == id<i32>(16);
    let not_equal = value != id<i32>(0);
    let less = value < id<i32>(20);
    let less_equal = value <= id<i32>(16);
    let ordering = value <=> id<i32>(16);
    let greater = value > id<i32>(4);
    let greater_equal = value >= id<i32>(16);
    let shift_left = value << id<i32>(1);
    let shift_right = value >> id<i32>(1);
    let add = value + id<i32>(1);
    let sub = value - id<i32>(1);
    let mul = value * id<i32>(2);
    let div = value / id<i32>(2);
    let rem = value % id<i32>(3);
    let casted = id<i32>(1) as i64;

    value = id<i32>(1);
    value += id<i32>(1);
    value -= id<i32>(1);
    value *= id<i32>(2);
    value /= id<i32>(2);
    value %= id<i32>(2);
    value &= id<i32>(1);
    value |= id<i32>(1);
    value ^= id<i32>(1);
    value <<= id<i32>(1);
    value >>= id<i32>(1);

    let positive = +value;
    let negative = -value;
    let logical = not true;
    let inverted = ~value;
    let address = &value;
    let loaded = *pointer;
    let alias = ref value;
    let moved = move value;
    let const_alias = const ref value;
    ++value;
    --value;
    value++;
    value--;
}
