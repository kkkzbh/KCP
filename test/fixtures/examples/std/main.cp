import std;

main() -> i32
{
    let some = optional<i32>::some(20);
    let none = optional<i32>::none;
    let value = expected<i32,str>::value(12);
    let error = expected<i32,str>::unexpected("bad");
    let indices = iota(0, 4);
    let total = 0;

    for(let index : indices) {
        total += index;
    }

    if(some.has_value() and not none.has_value() and value.has_value() and not error.has_value()) {
        return some.value_or(0) + none.value_or(10) + value.value_or(0) + total;
    }
    return 1;
}
