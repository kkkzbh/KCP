#pragma once

#include "case_io.hpp"

namespace test_parser {

auto inline compare_diagnostics(
    std::filesystem::path const& case_path,
    std::vector<expected_diagnostic> const& expected,
    std::vector<expected_diagnostic> const& actual) -> void
{
    if(expected == actual) {
        return;
    }

    auto expected_lines = std::vector<std::string>{};
    expected_lines.reserve(expected.size());
    for(auto const& value : expected) {
        expected_lines.push_back(format_diagnostic(value));
    }

    auto actual_lines = std::vector<std::string>{};
    actual_lines.reserve(actual.size());
    for(auto const& value : actual) {
        actual_lines.push_back(format_diagnostic(value));
    }

    fail(std::format (
        "diagnostic mismatch in {}\nexpected:\n{}actual:\n{}",
        std::filesystem::relative(case_path, cases_root()).string(),
        join_lines(expected_lines),
        join_lines(actual_lines)));
}

auto inline run_case(parser_case const& current_case) -> void
{
    auto sources = source_manager{};
    auto const file = sources.add_source (
        std::filesystem::relative(current_case.source_path, cases_root()).string(),
        current_case.source_text);

    auto result = parse_translation_unit(sources, file);
    if(result.accepted != current_case.accepted) {
        fail(std::format (
            "acceptance mismatch in {}: expected {}, got {}",
            std::filesystem::relative(current_case.source_path, cases_root()).string(),
            current_case.accepted ? "accepted" : "rejected",
            result.accepted ? "accepted" : "rejected"));
    }

    assert_true (
        result.lexer_diagnostics.empty(),
        std::format("{} should not emit lexer diagnostics", current_case.source_path.string()));
    if(current_case.accepted) {
        assert_true(result.root != std::nullopt, "accepted case should produce syntax tree");
    }

    auto actual = std::vector<expected_diagnostic>{};
    actual.reserve(result.parser_diagnostics.size());
    for(auto const& diagnostic : result.parser_diagnostics) {
        actual.push_back(to_expected_diagnostic(sources, diagnostic));
    }

    compare_diagnostics(current_case.source_path, current_case.diagnostics, actual);
}

auto inline run_case_suite(std::filesystem::path const& relative_root) -> int
{
    auto const cases = discover_cases(relative_root);
    for(auto const& current_case : cases) {
        run_case(current_case);
    }

    return 0;
}

} // namespace test_parser
