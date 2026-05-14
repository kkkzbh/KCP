#pragma once

#include "case_io.hpp"

namespace test_preprocessor {

auto inline compare_normalized_text(
    std::filesystem::path const& case_path,
    std::string const& expected,
    std::string const& actual) -> void
{
    if (expected == actual) {
        return;
    }

    fail(std::format (
        "normalized text mismatch in {}\nexpected:\n{}\nactual:\n{}\n",
        std::filesystem::relative(case_path, cases_root()).string(),
        expected,
        actual));
}

auto inline compare_issues(
    std::filesystem::path const& case_path,
    std::vector<expected_issue> const& expected,
    std::vector<expected_issue> const& actual) -> void
{
    if (expected == actual) {
        return;
    }

    auto expected_lines = std::vector<std::string>{};
    expected_lines.reserve(expected.size());
    for (auto const& value : expected) {
        expected_lines.push_back(format_issue(value));
    }

    auto actual_lines = std::vector<std::string>{};
    actual_lines.reserve(actual.size());
    for (auto const& value : actual) {
        actual_lines.push_back(format_issue(value));
    }

    fail(std::format (
        "issue mismatch in {}\nexpected:\n{}actual:\n{}",
        std::filesystem::relative(case_path, cases_root()).string(),
        join_lines(expected_lines),
        join_lines(actual_lines)));
}

auto inline run_case(preprocessor_case const& current_case) -> void
{
    auto sources = source_manager{};
    auto const file = sources.add_source (
        std::filesystem::relative(current_case.source_path, cases_root()).string(),
        current_case.source_text);

    auto const result = preprocess(sources, file);

    auto actual_issues = std::vector<expected_issue>{};
    actual_issues.reserve(result.issues.size());
    for (auto const& issue : result.issues) {
        actual_issues.push_back(to_expected_issue(sources, issue));
    }

    compare_normalized_text (
        current_case.source_path,
        current_case.normalized_text,
        result.normalized_text);
    compare_issues(current_case.source_path, current_case.issues, actual_issues);
}

auto inline run_case_suite(std::filesystem::path const& relative_root) -> int
{
    auto const cases = discover_cases(relative_root);
    for (auto const& current_case : cases) {
        run_case(current_case);
    }
    return 0;
}

} // namespace test_preprocessor
