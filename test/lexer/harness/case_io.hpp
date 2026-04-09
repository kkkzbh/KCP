#pragma once

#include "assert.hpp"
#include "case_types.hpp"

namespace test_lexer {

[[nodiscard]] inline auto cases_root() -> std::filesystem::path
{
    return std::filesystem::path{TEST_LEXER_CASES_DIR};
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

[[nodiscard]] inline auto normalize_source_text(
    std::filesystem::path const& path,
    std::string text) -> std::string
{
    auto const stem = path.stem().string();
    auto const should_trim_final_newline = stem != "unterminated_newline";

    if (should_trim_final_newline && text.ends_with('\n')) {
        text.pop_back();
    }

    return text;
}

[[nodiscard]] inline auto split_exact(std::string const& line, char delimiter)
    -> std::vector<std::string>
{
    auto fields = std::vector<std::string>{};
    auto start = 0uz;

    for (;;) {
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

[[nodiscard]] inline auto parse_token_kind(std::string const& spelling) -> token_kind
{
    for (auto const kind : all_token_kinds) {
        if (to_string(kind) == spelling) {
            return kind;
        }
    }

    fail(std::format("unknown token kind '{}'", spelling));
}

[[nodiscard]] inline auto parse_diagnostic_code(std::string const& spelling) -> diagnostic_code
{
    for (auto const code : all_diagnostic_codes) {
        if (diagnostic_code_name(code) == spelling) {
            return code;
        }
    }

    fail(std::format("unknown diagnostic code '{}'", spelling));
}

[[nodiscard]] inline auto parse_flags(std::string const& field) -> token_flags
{
    if (field == "-") {
        return token_flags::none;
    }

    auto flags = token_flags::none;
    auto const parts = split_exact(field, ',');
    for (auto const& part : parts) {
        if (part == "leading_space") {
            flags |= token_flags::leading_space;
            continue;
        }
        if (part == "start_of_line") {
            flags |= token_flags::start_of_line;
            continue;
        }
        if (part == "unterminated") {
            flags |= token_flags::unterminated;
            continue;
        }
        if (part == "recovered") {
            flags |= token_flags::recovered;
            continue;
        }

        fail(std::format("unknown token flag '{}'", part));
    }

    return flags;
}

[[nodiscard]] inline auto parse_expected_tokens(std::filesystem::path const& path)
    -> std::vector<expected_token>
{
    auto stream = std::ifstream{path};
    if (!stream.is_open()) {
        fail(std::format("missing token expectation file {}", path.string()));
    }

    auto result = std::vector<expected_token>{};
    auto line = std::string{};
    auto line_number = 0uz;
    while (std::getline(stream, line)) {
        ++line_number;
        if (line.empty()) {
            continue;
        }

        auto const fields = split_exact(line, '\t');
        if (fields.size() != 3) {
            fail(std::format(
                "{}:{} expected 3 tab-separated fields, got {}",
                path.string(),
                line_number,
                fields.size()));
        }

        result.push_back(expected_token{
            .kind = parse_token_kind(fields[0]),
            .lexeme = fields[1],
            .flags = parse_flags(fields[2]),
        });
    }

    assert_true(!result.empty(), std::format("{} should not be empty", path.string()));
    assert_true(result.back().kind == token_kind::eof,
        std::format("{} must end with eof token", path.string()));
    return result;
}

[[nodiscard]] inline auto parse_expected_diagnostics(std::filesystem::path const& path)
    -> std::vector<expected_diagnostic>
{
    if (!std::filesystem::exists(path)) {
        return {};
    }

    auto stream = std::ifstream{path};
    if (!stream.is_open()) {
        fail(std::format("failed to open {}", path.string()));
    }

    auto result = std::vector<expected_diagnostic>{};
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

        result.push_back(expected_diagnostic{
            .code = parse_diagnostic_code(fields[0]),
            .span_lexeme = fields[1],
        });
    }

    return result;
}

[[nodiscard]] inline auto discover_cases(std::filesystem::path const& relative_root)
    -> std::vector<lexer_case>
{
    auto const root = cases_root() / relative_root;
    assert_true(std::filesystem::exists(root), std::format("{} should exist", root.string()));

    auto paths = std::vector<std::filesystem::path>{};
    for (auto const& entry : std::filesystem::recursive_directory_iterator{root}) {
        if (entry.is_regular_file() && entry.path().extension() == ".lex") {
            paths.push_back(entry.path());
        }
    }

    std::ranges::sort(paths);
    assert_true(!paths.empty(), std::format("{} should contain cases", root.string()));

    auto result = std::vector<lexer_case>{};
    result.reserve(paths.size());

    for (auto const& source_path : paths) {
        auto const tokens_path = source_path.parent_path() / (source_path.stem().string() + ".tokens");
        auto const diag_path = source_path.parent_path() / (source_path.stem().string() + ".diag");

        if (result.empty()) {
            result.reserve(paths.size());
        }

        result.push_back(lexer_case{
            .source_path = source_path,
            .source_text = normalize_source_text(source_path, read_text(source_path)),
            .tokens = parse_expected_tokens(tokens_path),
            .diagnostics = parse_expected_diagnostics(diag_path),
        });
    }

    return result;
}

} // namespace test_lexer
