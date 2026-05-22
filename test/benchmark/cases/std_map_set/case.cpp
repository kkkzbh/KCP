#include <cstdint>
#include <cstdlib>
#include <map>
#include <set>

auto main() -> int
{
    auto const* raw = std::getenv("CP_BENCH_INPUT");
    if(raw == nullptr) {
        return 1;
    }

    auto const count = std::atoi(raw);
    auto values = std::map<int, int>{};
    auto keys = std::set<int>{};

    auto index = 0;
    while(index < count) {
        auto const key = (index * 37) % (count * 2 + 1);
        values.emplace(key, index % 1009);
        keys.emplace(key);
        index += 1;
    }

    auto total = static_cast<std::int64_t>(values.size() + keys.size());
    auto probe = 0;
    while(probe < count) {
        auto const key = (probe * 53) % (count * 2 + 1);
        if(auto iter = values.find(key); iter != values.end()) {
            total += iter->second;
        }
        if(keys.contains(key)) {
            total += 3;
        }
        probe += 1;
    }

    return static_cast<int>(total % 251);
}
