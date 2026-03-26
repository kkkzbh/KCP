import std;
import smoke;

auto main() -> int
{
    if (answer() != 42) {
        return 1;
    }

    const std::vector<int> values{1, 2, 3};
    return values.size() == 3 ? 0 : 2;
}
