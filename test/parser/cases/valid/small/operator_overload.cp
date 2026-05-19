export struct vec2 {
    x: i32;
    y: i32;
}

export operator +(left: vec2 const&, right: vec2 const&) -> vec2
{
    return vec2{ .x = left.x + right.x, .y = left.y + right.y };
}

impl vec2 {
    operator [](self&, index: i32) -> i32&
    {
        return x;
    }

    operator +=(self&, rhs: this const&)
    {
        x += rhs.x;
        y += rhs.y;
    }
}

