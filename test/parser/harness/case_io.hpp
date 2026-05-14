#pragma once

#include "assert.hpp"
#include "case_types.hpp"
#include "jsonl.hpp"

namespace test_parser {

[[nodiscard]]
auto inline cases_root() -> std::filesystem::path
{
    return std::filesystem::path{ TEST_PARSER_CASES_DIR };
}

[[nodiscard]]
auto inline read_text(std::filesystem::path const& path) -> std::string
{
    auto stream = std::ifstream{ path };
    if(not stream.is_open()) {
        fail(std::format("failed to open {}", path.string()));
    }

    auto buffer = std::ostringstream{};
    buffer << stream.rdbuf();
    return buffer.str();
}

[[nodiscard]]
auto inline normalize_source_text(std::string text) -> std::string
{
    if(text.ends_with('\n')) {
        text.pop_back();
    }
    return text;
}

[[nodiscard]]
auto inline parse_diagnostic_code(std::string const& spelling)
    -> parser_diagnostic_code
{
    for(auto const code : {
            parser_diagnostic_code::unexpected_token,
            parser_diagnostic_code::expected_token,
            parser_diagnostic_code::expected_identifier,
            parser_diagnostic_code::expected_expression,
            parser_diagnostic_code::expected_statement,
            parser_diagnostic_code::expected_type,
            parser_diagnostic_code::lexical_failure,
        }) {
        if(to_string(code) == spelling) {
            return code;
        }
    }

    fail(std::format("unknown parser diagnostic code '{}'", spelling));
}

[[nodiscard]]
auto inline parse_expected_diagnostics(std::filesystem::path const& path)
    -> std::vector<expected_diagnostic>
{
    auto result = std::vector<expected_diagnostic>{};
    for(auto const& record : test_support::read_jsonl(path, false)) {
        result.emplace_back (
            parse_diagnostic_code(test_support::required_string(record, path, "code")),
            test_support::required_string(record, path, "span"),
            test_support::required_size(record, path, "start"),
            test_support::required_size(record, path, "end"),
            test_support::required_size(record, path, "line"),
            test_support::required_size(record, path, "column"));
    }

    return result;
}

[[nodiscard]]
auto inline parse_expected_status(std::filesystem::path const& path) -> bool
{
    auto const text = read_text(path);
    if(text == "accepted" or text == "accepted\n") {
        return true;
    }
    if(text == "rejected" or text == "rejected\n") {
        return false;
    }

    fail(std::format("unexpected status content in {}", path.string()));
}

[[nodiscard]]
auto inline discover_cases(std::filesystem::path const& relative_root)
    -> std::vector<parser_case>
{
    auto const root = cases_root() / relative_root;
    assert_true(std::filesystem::exists(root), std::format("{} should exist", root.string()));

    auto paths = std::vector<std::filesystem::path>{};
    for(auto const& entry : std::filesystem::recursive_directory_iterator{ root }) {
        if(entry.is_regular_file() and entry.path().extension() == ".cp") {
            paths.push_back(entry.path());
        }
    }

    std::ranges::sort(paths);
    assert_true(not paths.empty(), std::format("{} should contain cases", root.string()));

    auto result = std::vector<parser_case>{};
    result.reserve(paths.size());
    for(auto const& source_path : paths) {
        auto const stem = source_path.stem().string();
        result.emplace_back (
            source_path,
            normalize_source_text(read_text(source_path)),
            parse_expected_status(source_path.parent_path() / (stem + ".status")),
            parse_expected_diagnostics(source_path.parent_path() / (stem + ".diag.jsonl")));
    }

    return result;
}

} // namespace test_parser
