export module smoke;

import std;

export auto answer() -> int
{
    const std::vector<int> values{19, 23};
    return std::accumulate(values.begin(), values.end(), 0);
}
