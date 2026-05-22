#include <cstdint>
#include <cstdlib>
#include <memory>

auto main() -> int
{
    auto const* raw = std::getenv("CP_BENCH_INPUT");
    if(raw == nullptr) {
        return 1;
    }

    auto const count = std::atoi(raw);
    auto values = std::make_unique<int[]>(count);
    auto index = 0;
    while(index < count) {
        values[index] = (index * 17 + 3) % 1009;
        index += 1;
    }

    auto total = std::int64_t{ 0 };
    auto* cursor = values.get();
    auto* end = values.get() + count;
    while(cursor < end) {
        total += *cursor;
        cursor += 1;
    }

    return static_cast<int>(total % 251);
}
