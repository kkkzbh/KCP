export module math;

export add(x: i32, const y: i32) -> i32
{
    return x + y;
}

export clamp_zero(value: i32) -> i32
{
    if(value < 0) {
        return 0;
    }

    return value;
}

export average(left: f64, right: f64) -> f64
{
    return (left + right) / 2.0;
}
