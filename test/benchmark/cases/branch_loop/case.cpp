#include <cstdlib>

auto main() -> int
{
    auto const* raw = std::getenv("CP_BENCH_INPUT");
    if(raw == nullptr) {
        return 1;
    }

    auto const limit = std::atoi(raw);
    auto index = 0;
    auto total = 0;

    while(index < limit) {
        if(index % 7 == 0) {
            total += 3;
        } else if(index % 5 == 0) {
            total += 2;
        } else {
            total += 1;
        }
        index += 1;
    }

    return total % 251;
}
