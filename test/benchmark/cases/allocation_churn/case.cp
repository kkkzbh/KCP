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
    let outer = 0;
    let total = 0;
    while(outer < count) {
        let values = alloc<i32>(32 as usize);
        let index = 0;
        while(index < 32) {
            *(values + index as usize) = (outer + index * 13) % 997;
            index += 1;
        }
        total = (total + *(values + (outer % 32) as usize)) % 1000003;
        free(values);
        outer += 1;
    }
    return total % 251;
}
