add(left: i32, right: i32) -> i32
{
    return left + right;
}

apply(value: i32, op: f(i32) -> i32) -> i32
{
    return op(value);
}

call_raw(op: f*(i32, i32) -> i32) -> i32
{
    return op(20, 1);
}

main() -> i32
{
    let inc: f(i32) -> i32 = fn(value) => value + 1;
    let bias = 1;
    let add_bias = fn(value: i32) -> i32 {
        value + bias
    };

    return apply(19, inc) + call_raw(add) + add_bias(0);
}
