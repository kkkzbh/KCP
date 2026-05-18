import std.option;
import std.result;

main() -> i32
{
    let some = optional<i32>::some(20);
    let none = optional<i32>::none;
    let ok = result<i32,str>::ok(12);
    let err = result<i32,str>::error("bad");

    if(some.is_some() and none.is_none() and ok.is_ok() and err.is_error()) {
        return some.value_or(0) + none.value_or(10) + ok.value_or(0);
    }
    return 1;
}
