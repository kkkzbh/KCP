#pragma once

#include "assert.hpp"
#include "case_types.hpp"
#include "jsonl.hpp"

namespace test_preprocessor {

[[nodiscard]]
auto inline cases_root() -> std::filesystem::path
{
    return std::filesystem::path{TEST_PREPROCESSOR_CASES_DIR};
}

[[nodiscard]]
auto inline read_text(std::filesystem::path const& path) -> std::string
{
    auto stream = std::ifstream{path};
    if (not stream.is_open()) {
        fail(std::format("failed to open {}", path.string()));
    }

    auto buffer = std::ostringstream{};
    buffer << stream.rdbuf();
    return buffer.str();
}

[[nodiscard]]
auto inline normalize_case_text(std::string text) -> std::string
{
    if (text.ends_with('\n')) {
        text.pop_back();
    }

    return text;
}

[[nodiscard]]
auto inline split_exact(std::string const& line, char delimiter)
    -> std::vector<std::string>
{
    auto fields = std::vector<std::string>{};
    auto start = 0uz;

    while (true) {
        auto const index = line.find(delimiter, start);
        if (index == std::string::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, index - start));
        start = index + 1;
    }

    return fields;
}

[[nodiscard]]
auto inline unescape_field(std::string_view text) -> std::string
{
    auto result = std::string{};
    result.reserve(text.size());

    for (auto index = 0uz; index < text.size(); ++index) {
        auto const ch = text[index];
        if (ch != '\\' or index + 1 >= text.size()) {
            result.push_back(ch);
            continue;
        }

        ++index;
        switch (text[index]) {
        case '\\':
            result.push_back('\\');
            break;
        case 'n':
            result.push_back('\n');
            break;
        case 't':
            result.push_back('\t');
            break;
        default:
            fail(std::format("unknown escape sequence '\\{}'", text[index]));
        }
    }

    return result;
}

[[nodiscard]]
auto inline parse_diagnostic_kind(std::string const& spelling)
    -> diagnostic_kind
{
    if (spelling == "unterminated_block_comment") {
        return diagnostic_kind::unterminated_block_comment;
    }

    fail(std::format("unknown preprocessor diagnostic kind '{}'", spelling));
}

[[nodiscard]]
auto inline parse_expected_diagnostics(std::filesystem::path const& path)
    -> std::vector<expected_diagnostic>
{
    auto result = std::vector<expected_diagnostic>{};
    for (auto const& record : test_support::read_jsonl(path, false)) {
        result.emplace_back (
            parse_diagnostic_kind(test_support::required_string(record, path, "code")),
            test_support::required_string(record, path, "span"),
            test_support::required_size(record, path, "start"),
            test_support::required_size(record, path, "end"),
            test_support::required_size(record, path, "line"),
            test_support::required_size(record, path, "column"));
    }

    return result;
}

[[nodiscard]]
auto inline discover_cases(std::filesystem::path const& relative_root)
    -> std::vector<preprocessor_case>
{
    auto const root = cases_root() / relative_root;
    assert_true(std::filesystem::exists(root), std::format("{} should exist", root.string()));

    auto paths = std::vector<std::filesystem::path>{};
    for (auto const& entry : std::filesystem::recursive_directory_iterator{root}) {
        if (entry.is_regular_file() and entry.path().extension() == ".cp") {
            paths.push_back(entry.path());
        }
    }

    std::ranges::sort(paths);
    assert_true(not paths.empty(), std::format("{} should contain cases", root.string()));

    auto result = std::vector<preprocessor_case>{};
    result.reserve(paths.size());

    for (auto const& source_path : paths) {
        auto const output_path = source_path.parent_path() / (source_path.stem().string() + ".out");
        auto const diagnostics_path = (
            source_path.parent_path() / (source_path.stem().string() + ".diagnostics.jsonl")
        );

        assert_true(std::filesystem::exists(output_path),
            std::format("missing normalized output file {}", output_path.string()));

        result.emplace_back (
            source_path,
            normalize_case_text(read_text(source_path)),
            normalize_case_text(read_text(output_path)),
            parse_expected_diagnostics(diagnostics_path));
    }

    return result;
}

} // namespace test_preprocessor
