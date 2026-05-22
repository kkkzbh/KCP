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
    let values = map<i32, i32>{};
    let keys = set<i32>{};

    let index = 0;
    while(index < count) {
        let key = (index * 37) % (count * 2 + 1);
        values.insert(key, index % 1009);
        keys.insert(key);
        index += 1;
    }

    let total: i64 = values.size() as i64 + keys.size() as i64;
    let probe = 0;
    while(probe < count) {
        let key = (probe * 53) % (count * 2 + 1);
        if(values.contains(key)) {
            total += values.at(key) as i64;
        }
        if(keys.contains(key)) {
            total += 3;
        }
        probe += 1;
    }

    return (total % 251) as i32;
}
