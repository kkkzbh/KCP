import std.algorithm.sort;
import std.collections.vector;
import std.text.str;

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

stable_requested() -> bool
{
    let name = "CP_BENCH_STABLE";
    return getenv(name.data()) != nullptr;
}

repeat_count(count: i32) -> i32
{
    if(count <= 2048) {
        return 512;
    }
    if(count <= 65536) {
        return 32;
    }
    return 2;
}

value_at(index: i32, count: i32) -> i32
{
    return (((index as i64) * 48271 + (count as i64) * 17) % 1000003) as i32;
}

checked_checksum(values: vector<i32> const&) -> i64
{
    let total: i64 = 0;
    let index: usize = 0;
    while(index < values.size()) {
        if(index > 0 as usize and values[index] < values[index - 1]) {
            return -1;
        }
        total = (total + (values[index] as i64) * ((index % 251 + 1) as i64)) % 1000000007;
        index += 1;
    }
    return total;
}

main() -> i32
{
    let count = bench_input();
    if(count <= 0) {
        return 1;
    }

    let original = vector<i32>{};
    original.reserve(count as usize);
    let index = 0;
    while(index < count) {
        original.push_back(value_at(index, count));
        index += 1;
    }

    let total: i64 = 0;
    let round = 0;
    let repeats = repeat_count(count);
    let stable = stable_requested();
    while(round < repeats) {
        let values = original;
        if(stable) {
            stable_sort(values);
        } else {
            sort(values);
        }
        let checksum = checked_checksum(values);
        if(checksum < 0) {
            return 2;
        }
        total = (total + checksum) % 1000000007;
        round += 1;
    }

    return (total % 251) as i32;
}
