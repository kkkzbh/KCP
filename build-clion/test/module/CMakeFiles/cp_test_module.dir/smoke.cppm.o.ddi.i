# 0 "/home/kkkzbh/code/cp/test/module/smoke.cppm"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "/home/kkkzbh/code/cp/test/module/smoke.cppm"
export module smoke;

import std;

export auto answer() -> int
{
    const std::vector<int> values{19, 23};
    return std::accumulate(values.begin(), values.end(), 0);
}
