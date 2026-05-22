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

mix<T>(value: T, salt: T) -> T
{
    return (value * 17 + salt) % 1009;
}

main() -> i32
{
    let count = bench_input();
    let index = 0;
    let total_i32 = 0;
    let total_i64: i64 = 0;
    while(index < count) {
        total_i32 = (total_i32 + mix<i32>(index, 11)) % 1000003;
        total_i64 = (total_i64 + mix<i64>(index as i64, 23)) % 1000003;
        index += 1;
    }
    return ((total_i32 as i64 + total_i64) % 251) as i32;
}
