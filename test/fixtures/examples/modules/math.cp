export module sample.math;

export add(left: i32, right: i32) -> i32
{
    return left + right;
}

export clamp_min(value: i32, minimum: i32) -> i32
{
    if(value < minimum) {
        return minimum;
    }

    return value;
}
