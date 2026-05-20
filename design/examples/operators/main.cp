struct vec2 {
    x: i32;
    y: i32;
}

impl vec2 {
    operator +(self const&, rhs: this const&) -> this
    {
        return vec2{ .x = x + rhs.x, .y = y + rhs.y };
    }

    operator +=(self&, rhs: this const&)
    {
        x += rhs.x;
        y += rhs.y;
    }
}

struct cell {
    value: i32;
}

impl cell {
    operator [](self&, index: i32) -> i32&
    {
        return value;
    }

    operator =(self&, rhs: this const&)
    {
        value = rhs.value + 5;
    }
}

struct scaler {
    factor: i32;
}

impl scaler {
    operator ()(self const&, value: i32) -> i32
    {
        return value * factor;
    }
}

main() -> i32
{
    let a = vec2{ 1, 2 };
    let b = vec2{ 3, 4 };
    let c = a + b;
    a += b;

    let item = cell{ 0 };
    item[0] = c.x + a.y;
    let other = cell{ 5 };
    item = other;

    let twice = scaler{ 2 };
    return item[0] + twice(16);
}
