import std;
import source;
import preprocessor;

#include "runner.hpp"

auto main() -> int
{
    return test_preprocessor::run_case_suite("invalid");
}
