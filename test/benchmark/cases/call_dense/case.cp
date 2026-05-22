import std;

extern "C" getenv(name: char const*) -> char*;
extern "C" atoi(text: char const*) -> i32;

bench_input() -> i32
{
    let name = "CP_BENCH_INPUT";
    let raw = getenv(name.data());
    if(raw == nullptr) {
        return 1;
    }
    return atoi(raw);
}

step_a(value: i32) -> i32
{
    return (value * 3 + 1) % 1009;
}

step_b(value: i32) -> i32
{
    return (step_a(value) + step_a(value + 7)) % 1009;
}

step_c(value: i32) -> i32
{
    return (step_b(value) * 5 + step_a(value - 3)) % 1009;
}

main() -> i32
{
    let count = bench_input();
    let index = 0;
    let total = 0;
    while(index < count) {
        total = (total + step_c(index)) % 1000003;
        index += 1;
    }
    return total % 251;
}
