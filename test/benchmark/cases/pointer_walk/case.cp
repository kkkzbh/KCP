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
    let count = bench_input();
    let values = alloc<i32>(count as usize);
    let index: i32 = 0;

    while(index < count) {
        *(values + index as usize) = (index * 17 + 3) % 1009;
        index += 1;
    }

    let total: i64 = 0;
    let cursor = 0;
    while(cursor < count) {
        total += *(values + cursor as usize) as i64;
        cursor += 1;
    }

    free(values);
    return (total % 251) as i32;
}
