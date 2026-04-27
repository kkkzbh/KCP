#pragma once

namespace test_preprocessor {

[[noreturn]] inline auto fail(std::string message) -> void
{
    std::cerr << message << '\n';
    std::exit(1);
}

inline auto assert_true(bool condition, std::string_view message) -> void
{
    if (not condition) {
        fail("assertion failed: " + std::string(message));
    }
}

} // namespace test_preprocessor
