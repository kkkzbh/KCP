#pragma once

#include "case_io.hpp"

namespace test_lexer {

inline auto compare_tokens(
    std::filesystem::path const& case_path,
    std::vector<expected_token> const& expected,
    std::vector<expected_token> const& actual) -> void
{
    if (expected == actual) {
        return;
    }

    auto expected_lines = std::vector<std::string>{};
    expected_lines.reserve(expected.size());
    for (auto const& value : expected) {
        expected_lines.push_back(format_token(value));
    }

    auto actual_lines = std::vector<std::string>{};
    actual_lines.reserve(actual.size());
    for (auto const& value : actual) {
        actual_lines.push_back(format_token(value));
    }

    fail(std::format(
        "token mismatch in {}\nexpected:\n{}actual:\n{}",
        std::filesystem::relative(case_path, cases_root()).string(),
        join_lines(expected_lines),
        join_lines(actual_lines)));
}

inline auto compare_diagnostics(
    std::filesystem::path const& case_path,
    std::vector<expected_diagnostic> const& expected,
    std::vector<expected_diagnostic> const& actual) -> void
{
    if (expected == actual) {
        return;
    }

    auto expected_lines = std::vector<std::string>{};
    expected_lines.reserve(expected.size());
    for (auto const& value : expected) {
        expected_lines.push_back(format_diagnostic(value));
    }

    auto actual_lines = std::vector<std::string>{};
    actual_lines.reserve(actual.size());
    for (auto const& value : actual) {
        actual_lines.push_back(format_diagnostic(value));
    }

    fail(std::format(
        "diagnostic mismatch in {}\nexpected:\n{}actual:\n{}",
        std::filesystem::relative(case_path, cases_root()).string(),
        join_lines(expected_lines),
        join_lines(actual_lines)));
}

inline auto run_case(lexer_case const& current_case) -> void
{
    auto sources = front::source_manager{};
    auto diagnostics = front::vector_diagnostic_sink{};
    auto const file = sources.add_source(
        std::filesystem::relative(current_case.source_path, cases_root()).string(),
        current_case.source_text);

    auto lex = front::lexer{sources, file, diagnostics};
    auto const tokens = lex.tokenize_all();

    auto actual_tokens = std::vector<expected_token>{};
    actual_tokens.reserve(tokens.size());
    for (auto const& token : tokens) {
        actual_tokens.push_back(to_expected_token(sources, token));
    }

    auto actual_diagnostics = std::vector<expected_diagnostic>{};
    actual_diagnostics.reserve(diagnostics.diagnostics().size());
    for (auto const& diagnostic : diagnostics.diagnostics()) {
        actual_diagnostics.push_back(to_expected_diagnostic(sources, diagnostic));
    }

    compare_tokens(current_case.source_path, current_case.tokens, actual_tokens);
    compare_diagnostics(current_case.source_path, current_case.diagnostics, actual_diagnostics);
}

inline auto run_case_suite(std::filesystem::path const& relative_root) -> int
{
    auto const cases = discover_cases(relative_root);
    for (auto const& current_case : cases) {
        run_case(current_case);
    }
    return 0;
}

} // namespace test_lexer
