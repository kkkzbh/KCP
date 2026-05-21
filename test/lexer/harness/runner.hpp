#pragma once

#include "case_io.hpp"

namespace test_lexer {

auto inline compare_tokens(std::filesystem::path const& case_path, std::vector<expected_token> const& expected, std::vector<expected_token> const& actual) -> void
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

    fail(std::format (
        "token mismatch in {}\nexpected:\n{}actual:\n{}",
        std::filesystem::relative(case_path, cases_root()).string(),
        join_lines(expected_lines),
        join_lines(actual_lines)));
}

auto inline compare_diagnostics(std::filesystem::path const& case_path, std::vector<expected_diagnostic> const& expected, std::vector<expected_diagnostic> const& actual) -> void
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

    fail(std::format (
        "diagnostic mismatch in {}\nexpected:\n{}actual:\n{}",
        std::filesystem::relative(case_path, cases_root()).string(),
        join_lines(expected_lines),
        join_lines(actual_lines)));
}

auto inline assert_invalid_tokens_match_diagnostics(std::vector<token> const& tokens, std::vector<diagnostic> const& diagnostics) -> void
{
    auto invalid_tokens = std::vector<token const*>{};
    for(auto const& value : tokens) {
        if(value.kind == token_kind::invalid) {
            invalid_tokens.push_back(&value);
        }
    }

    contract_assert(invalid_tokens.empty() == diagnostics.empty());
    contract_assert(invalid_tokens.size() == diagnostics.size());

    for(auto const& diagnostic : diagnostics) {
        auto const covered = std::ranges::any_of(invalid_tokens, [&](token const* value) {
            return value->span.start <= diagnostic.primary_span.start
                and diagnostic.primary_span.end <= value->span.end;
        });

        contract_assert(covered);
    }
}

auto inline run_case(lexer_case const& current_case) -> void
{
    auto sources = source_manager{};
    auto const file = sources.add_source (
        std::filesystem::relative(current_case.source_path, cases_root()).string(),
        current_case.source_text);

    auto preprocessed = preprocess(sources, file);
    contract_assert(preprocessed.diagnostics.empty());
    auto result = lex(preprocessed);
    assert_invalid_tokens_match_diagnostics(result.tokens, result.diagnostics);

    auto actual_tokens = std::vector<expected_token>{};
    actual_tokens.reserve(result.tokens.size());
    for (auto const& token : result.tokens) {
        actual_tokens.push_back(to_expected_token(sources, token));
    }

    auto actual_diagnostics = std::vector<expected_diagnostic>{};
    actual_diagnostics.reserve(result.diagnostics.size());
    for (auto const& diagnostic : result.diagnostics) {
        actual_diagnostics.push_back(to_expected_diagnostic(sources, diagnostic));
    }

    compare_tokens(current_case.source_path, current_case.tokens, actual_tokens);
    compare_diagnostics(current_case.source_path, current_case.diagnostics, actual_diagnostics);
}

auto inline run_case_suite(std::filesystem::path const& relative_root) -> int
{
    auto const cases = discover_cases(relative_root);
    for (auto const& current_case : cases) {
        run_case(current_case);
    }
    return 0;
}

} // namespace test_lexer
