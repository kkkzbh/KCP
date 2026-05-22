#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

auto main() -> int
{
    auto const* raw = std::getenv("CP_BENCH_INPUT");
    if(raw == nullptr) {
        return 1;
    }

    auto const count = std::atoi(raw);
    auto values = std::vector<int>{};
    auto text = std::string{};
    values.reserve(static_cast<std::size_t>(count));
    text.reserve(static_cast<std::size_t>(count));

    auto index = 0;
    while(index < count) {
        values.push_back((index * 13 + 5) % 997);
        text.push_back(static_cast<char>('a' + (index % 26)));
        index += 1;
    }

    auto total = static_cast<std::int64_t>(text.size());
    auto cursor = std::size_t{ 0 };
    while(cursor < values.size()) {
        total += values[cursor];
        cursor += 1;
    }
    return static_cast<int>(total % 251);
}
