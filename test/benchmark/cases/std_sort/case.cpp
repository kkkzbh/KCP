#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

auto repeat_count(int count) -> int
{
    if(count <= 2048) {
        return 512;
    }
    if(count <= 65536) {
        return 32;
    }
    return 2;
}

auto stable_requested() -> bool
{
    return std::getenv("CP_BENCH_STABLE") != nullptr;
}

auto value_at(int index, int count) -> int
{
    return static_cast<int>((static_cast<std::int64_t>(index) * 48271 + static_cast<std::int64_t>(count) * 17) % 1000003);
}

auto checked_checksum(std::vector<int> const& values) -> std::int64_t
{
    auto total = std::int64_t{ 0 };
    auto index = std::size_t{ 0 };
    while(index < values.size()) {
        if(index != 0 and values[index] < values[index - 1]) {
            return -1;
        }
        total = (total + static_cast<std::int64_t>(values[index]) * static_cast<std::int64_t>(index % 251 + 1)) % 1'000'000'007;
        index += 1;
    }
    return total;
}

auto main() -> int
{
    auto const* raw = std::getenv("CP_BENCH_INPUT");
    if(raw == nullptr) {
        return 1;
    }

    auto const count = std::atoi(raw);
    if(count <= 0) {
        return 1;
    }

    auto original = std::vector<int>{};
    original.reserve(static_cast<std::size_t>(count));
    auto index = 0;
    while(index < count) {
        original.push_back(value_at(index, count));
        index += 1;
    }

    auto total = std::int64_t{ 0 };
    auto round = 0;
    auto const repeats = repeat_count(count);
    auto const stable = stable_requested();
    while(round < repeats) {
        auto values = original;
        if(stable) {
            std::ranges::stable_sort(values);
        } else {
            std::ranges::sort(values);
        }
        auto const checksum = checked_checksum(values);
        if(checksum < 0) {
            return 2;
        }
        total = (total + checksum) % 1'000'000'007;
        round += 1;
    }

    return static_cast<int>(total % 251);
}
