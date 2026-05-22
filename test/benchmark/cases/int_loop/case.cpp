#include <cstdint>
#include <cstdlib>

auto main() -> int
{
    auto const* raw = std::getenv("CP_BENCH_INPUT");
    if(raw == nullptr) {
        return 1;
    }

    auto const limit = std::atoi(raw);
    auto index = std::int64_t{ 0 };
    auto total = std::int64_t{ 7 };

    while(index < static_cast<std::int64_t>(limit)) {
        total = (total + ((index * 31 + 17) % 1009)) % 1000003;
        index += 1;
    }

    return static_cast<int>(total % 251);
}
