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

main() -> i32
{
    let limit = bench_input();
    let index: i64 = 0;
    let total: i64 = 7;

    while(index < limit as i64) {
        total = (total + ((index * 31 + 17) % 1009)) % 1000003;
        index += 1;
    }

    return (total % 251) as i32;
}
