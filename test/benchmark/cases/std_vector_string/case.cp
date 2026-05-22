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
    let values = vector<i32>{};
    let text = string{};
    values.reserve(count as usize);
    text.reserve(count as usize);

    let index = 0;
    while(index < count) {
        values.push_back((index * 13 + 5) % 997);
        text.push_back('a');
        index += 1;
    }

    let total: i64 = text.size() as i64;
    let cursor: usize = 0;
    while(cursor < values.size()) {
        total += values[cursor] as i64;
        cursor += 1;
    }
    return (total % 251) as i32;
}
