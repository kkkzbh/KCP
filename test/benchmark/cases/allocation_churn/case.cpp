#include <cstdlib>
#include <memory>

auto main() -> int
{
    auto const* raw = std::getenv("CP_BENCH_INPUT");
    if(raw == nullptr) {
        return 1;
    }

    auto const count = std::atoi(raw);
    auto outer = 0;
    auto total = 0;
    while(outer < count) {
        auto values = std::make_unique<int[]>(32);
        auto index = 0;
        while(index < 32) {
            values[index] = (outer + index * 13) % 997;
            index += 1;
        }
        total = (total + values[outer % 32]) % 1000003;
        outer += 1;
    }
    return total % 251;
}
