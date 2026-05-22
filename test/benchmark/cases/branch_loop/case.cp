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
    let index: i32 = 0;
    let total: i32 = 0;

    while(index < limit) {
        if(index % 7 == 0) {
            total += 3;
        } else if(index % 5 == 0) {
            total += 2;
        } else {
            total += 1;
        }
        index += 1;
    }

    return total % 251;
}
