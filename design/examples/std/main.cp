import std;

main() -> i32
{
    let some = optional<i32>::some(20);
    let none = optional<i32>::none;
    let value = expected<i32,str>::value(12);
    let error = expected<i32,str>::unexpected("bad");

    if(some.has_value() and not none.has_value() and value.has_value() and not error.has_value()) {
        return some.value_or(0) + none.value_or(10) + value.value_or(0);
    }
    return 1;
}
