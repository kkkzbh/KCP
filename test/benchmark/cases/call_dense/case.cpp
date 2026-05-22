#include <cstdlib>

auto step_a(int value) -> int
{
    return (value * 3 + 1) % 1009;
}

auto step_b(int value) -> int
{
    return (step_a(value) + step_a(value + 7)) % 1009;
}

auto step_c(int value) -> int
{
    return (step_b(value) * 5 + step_a(value - 3)) % 1009;
}

auto main() -> int
{
    auto const* raw = std::getenv("CP_BENCH_INPUT");
    if(raw == nullptr) {
        return 1;
    }

    auto const count = std::atoi(raw);
    auto index = 0;
    auto total = 0;
    while(index < count) {
        total = (total + step_c(index)) % 1000003;
        index += 1;
    }
    return total % 251;
}
