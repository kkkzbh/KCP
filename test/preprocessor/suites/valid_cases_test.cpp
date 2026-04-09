import std;
import lexer.source;
import preprocessor.core;

#include "runner.hpp"

auto main() -> int
{
    return test_preprocessor::run_case_suite("valid");
}
