#pragma once

namespace test_parser {

[[noreturn]]
auto inline fail(std::string message) -> void
{
    std::cerr << message << '\n';
    std::exit(1);
}

auto inline assert_true(bool condition, std::string_view message) -> void
{
    if(not condition) {
        fail("assertion failed: " + std::string(message));
    }
}

} // namespace test_parser
