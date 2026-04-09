#pragma once

#include "assert.hpp"
#include "case_types.hpp"

namespace test_preprocessor {

[[nodiscard]] inline auto cases_root() -> std::filesystem::path
{
    return std::filesystem::path{TEST_PREPROCESSOR_CASES_DIR};
}

[[nodiscard]] inline auto read_text(std::filesystem::path const& path) -> std::string
{
    auto stream = std::ifstream{path};
    if (!stream.is_open()) {
        fail(std::format("failed to open {}", path.string()));
    }

    auto buffer = std::ostringstream{};
    buffer << stream.rdbuf();
    return buffer.str();
}

[[nodiscard]] inline auto normalize_case_text(std::string text) -> std::string
{
    if (text.ends_with('\n')) {
        text.pop_back();
    }

    return text;
}

[[nodiscard]] inline auto split_exact(std::string const& line, char delimiter)
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

[[nodiscard]] inline auto unescape_field(std::string_view text) -> std::string
{
    auto result = std::string{};
    result.reserve(text.size());

    for (auto index = 0uz; index < text.size(); ++index) {
        auto const ch = text[index];
        if (ch != '\\' || index + 1 >= text.size()) {
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

[[nodiscard]] inline auto parse_issue_kind(std::string const& spelling)
    -> preprocess_issue_kind
{
    if (spelling == "unterminated_block_comment") {
        return preprocess_issue_kind::unterminated_block_comment;
    }

    fail(std::format("unknown issue kind '{}'", spelling));
}

[[nodiscard]] inline auto parse_expected_issues(std::filesystem::path const& path)
    -> std::vector<expected_issue>
{
    if (!std::filesystem::exists(path)) {
        return {};
    }

    auto stream = std::ifstream{path};
    if (!stream.is_open()) {
        fail(std::format("failed to open {}", path.string()));
    }

    auto result = std::vector<expected_issue>{};
    auto line = std::string{};
    auto line_number = 0uz;
    while (std::getline(stream, line)) {
        ++line_number;
        if (line.empty()) {
            continue;
        }

        auto const fields = split_exact(line, '\t');
        if (fields.size() != 2) {
            fail(std::format(
                "{}:{} expected 2 tab-separated fields, got {}",
                path.string(),
                line_number,
                fields.size()));
        }

        result.push_back(expected_issue{
            .kind = parse_issue_kind(fields[0]),
            .span_lexeme = unescape_field(fields[1]),
        });
    }

    return result;
}

[[nodiscard]] inline auto discover_cases(std::filesystem::path const& relative_root)
    -> std::vector<preprocessor_case>
{
    auto const root = cases_root() / relative_root;
    assert_true(std::filesystem::exists(root), std::format("{} should exist", root.string()));

    auto paths = std::vector<std::filesystem::path>{};
    for (auto const& entry : std::filesystem::recursive_directory_iterator{root}) {
        if (entry.is_regular_file() && entry.path().extension() == ".cp") {
            paths.push_back(entry.path());
        }
    }

    std::ranges::sort(paths);
    assert_true(!paths.empty(), std::format("{} should contain cases", root.string()));

    auto result = std::vector<preprocessor_case>{};
    result.reserve(paths.size());

    for (auto const& source_path : paths) {
        auto const output_path = source_path.parent_path() / (source_path.stem().string() + ".out");
        auto const issues_path = source_path.parent_path() / (source_path.stem().string() + ".issues");

        assert_true(std::filesystem::exists(output_path),
            std::format("missing normalized output file {}", output_path.string()));

        result.push_back(preprocessor_case{
            .source_path = source_path,
            .source_text = normalize_case_text(read_text(source_path)),
            .normalized_text = normalize_case_text(read_text(output_path)),
            .issues = parse_expected_issues(issues_path),
        });
    }

    return result;
}

} // namespace test_preprocessor
