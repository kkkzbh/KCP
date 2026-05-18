struct vec2 {
    x: i32;
    y: i32;
}

impl vec2 {
    vec2() = default;

    vec2(x: i32, y: i32)
    {
        return vec2{ .x = x, .y = y };
    }

    move_by(self: vec2&, dx: i32, dy: i32)
    {
        x += dx;
        self.y += dy;
    }

    sum(self: vec2 const&) -> i32
    {
        return x + self.y;
    }

    zero() -> vec2
    {
        return vec2{};
    }
}

struct scope_mark {
    total: i32*;
    value: i32;
}

impl scope_mark {
    ~scope_mark()
    {
        *total += value;
    }
}

cleanup_score() -> i32
{
    let total = 0;
    {
        let first = scope_mark{ .total = &total, .value = 10 };
        let second = scope_mark{ .total = &total, .value = 20 };
    }
    return total;
}

main() -> i32
{
    let origin = vec2::zero();
    let point = vec2{ 3, 4 };
    let adjusted = {
        let copy = vec2{ .x = point.x, .y = origin.y };
        copy.move_by(9, 0);
        copy.sum()
    };

    return adjusted + cleanup_score();
}
