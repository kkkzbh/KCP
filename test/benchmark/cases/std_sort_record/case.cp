import std.algorithm.sort;
import std.collections.vector;
import std.compare;
import std.text.str;

extern "C" getenv(name: char const*) -> char*;
extern "C" atoi(text: char const*) -> i32;

struct record {
    key: i32;
    order: i32;
    payload: i64;
}

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
        return 256;
    }
    if(count <= 65536) {
        return 16;
    }
    return 2;
}

key_at(index: i32, count: i32) -> i32
{
    return (((index as i64) * 48271 + (count as i64) * 97) % 4096) as i32;
}

payload_at(index: i32, count: i32) -> i64
{
    return ((index as i64) * 1103515245 + (count as i64) * 12345) % 1000000007;
}

checked_checksum(values: vector<record> const&) -> i64
{
    let total: i64 = 0;
    let index: usize = 0;
    while(index < values.size()) {
        if(index > 0 as usize and values[index].key < values[index - 1].key) {
            return -1;
        }
        total = (total + (values[index].key as i64) * 17 + (values[index].order as i64) * 31 + values[index].payload) % 1000000007;
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

    let original = vector<record>{};
    original.reserve(count as usize);
    let index = 0;
    while(index < count) {
        original.push_back(record{ .key = key_at(index, count), .order = index, .payload = payload_at(index, count) });
        index += 1;
    }

    let total: i64 = 0;
    let round = 0;
    let repeats = repeat_count(count);
    let stable = stable_requested();
    while(round < repeats) {
        let values = original;
        if(stable) {
            stable_sort(values, f(left: record const&, right: record const&) -> weak_ordering {
                return left.key <=> right.key;
            });
        } else {
            sort(values, f(left: record const&, right: record const&) -> weak_ordering {
                return left.key <=> right.key;
            });
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
