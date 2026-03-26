# 0 "/home/kkkzbh/code/cp/test/module/main.cpp"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "/home/kkkzbh/code/cp/test/module/main.cpp"
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
