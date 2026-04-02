export module smoke;

import std;

export auto answer() -> int
{
    auto const values = std::vector{19, 23};
    return std::accumulate(values.begin(), values.end(), 0);
}
