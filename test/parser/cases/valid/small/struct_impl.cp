export struct vec2 {
    x: i32;
    y: i32;
}

impl vec2 {
    vec2() = default;

    vec2(x: i32, y: i32)
    {
        return vec2{ .x = x, .y = y };
    }

    ~vec2()
    {
    }

    sum(self const&) -> i32
    {
        return x + self.y;
    }

    zero() -> vec2
    {
        return vec2{};
    }
}

main() -> i32
{
    let value = vec2{ 1, 2 };
    return value.sum() + vec2::zero().x;
}
