import std.core.option;

fail(message: str) -> !
{
    panic(message);
}

value_or_fail(value: optional<i32>) -> i32
{
    return match value {
        .some(item) => item,
        .none => fail("missing value"),
    };
}

main() -> i32
{
    assert(true, "example precondition");
    return value_or_fail(optional<i32>::some(42));
}
