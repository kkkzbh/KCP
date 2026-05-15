import std;
import source;
import preprocessor;
import lexer;

#include "runner.hpp"

auto main() -> int
{
    return test_lexer::run_case_suite("invalid");
}
