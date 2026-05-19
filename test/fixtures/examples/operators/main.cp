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
}

main() -> i32
{
    let a = vec2{ 1, 2 };
    let b = vec2{ 3, 4 };
    let c = a + b;
    a += b;

    let item = cell{ 0 };
    item[0] = c.x + a.y;
    return item[0] + 32;
}

