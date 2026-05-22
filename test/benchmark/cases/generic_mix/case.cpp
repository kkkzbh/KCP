#include <cstdint>
#include <cstdlib>

template<typename T>
auto mix(T value, T salt) -> T
{
    return (value * 17 + salt) % 1009;
}

auto main() -> int
{
    auto const* raw = std::getenv("CP_BENCH_INPUT");
    if(raw == nullptr) {
        return 1;
    }

    auto const count = std::atoi(raw);
    auto index = 0;
    auto total_i32 = 0;
    auto total_i64 = std::int64_t{ 0 };
    while(index < count) {
        total_i32 = (total_i32 + mix<int>(index, 11)) % 1000003;
        total_i64 = (total_i64 + mix<std::int64_t>(index, 23)) % 1000003;
        index += 1;
    }
    return static_cast<int>((total_i32 + total_i64) % 251);
}
