import std;
import preprocessor;
import lexer;
import parser;

#include "runner.hpp"

auto main() -> int
{
    return test_parser::run_case_suite("invalid");
}
