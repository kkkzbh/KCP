import std.core.option;

variant event {
    quit;
    key(char);
    resize(i32, i32);
}

score(value: event) -> i32
{
    return match value {
        .resize(width, height) => width + height,
        .key(code) => 1,
        .quit => 0,
    };
}

main() -> i32
{
    let some = optional<i32>::some(20);
    let none = optional<i32>::none;
    return some.value_or(0) + none.value_or(10) + score(event::resize(5, 7));
}
