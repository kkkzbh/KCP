#pragma once

#include "assert.hpp"
#include "case_types.hpp"

namespace test_parser {

[[nodiscard]] inline auto cases_root() -> std::filesystem::path
{
    return std::filesystem::path{ TEST_PARSER_CASES_DIR };
}

[[nodiscard]] inline auto read_text(std::filesystem::path const& path) -> std::string
{
    auto stream = std::ifstream{ path };
    if(!stream.is_open()) {
        fail(std::format("failed to open {}", path.string()));
    }

    auto buffer = std::ostringstream{};
    buffer << stream.rdbuf();
    return buffer.str();
}

[[nodiscard]] inline auto normalize_source_text(std::string text) -> std::string
{
    if(text.ends_with('\n')) {
        text.pop_back();
    }
    return text;
}

[[nodiscard]] inline auto split_exact(std::string const& line, char delimiter)
    -> std::vector<std::string>
{
    auto parts = std::vector<std::string>{};
    auto start = 0uz;
    while(true) {
        auto const index = line.find(delimiter, start);
        if(index == std::string::npos) {
            parts.push_back(line.substr(start));
            break;
        }
        parts.push_back(line.substr(start, index - start));
        start = index + 1;
    }

    return parts;
}

[[nodiscard]] inline auto parse_expected_diagnostics(std::filesystem::path const& path)
    -> std::vector<expected_diagnostic>
{
    if(!std::filesystem::exists(path)) {
        return {};
    }

    auto stream = std::ifstream{ path };
    if(!stream.is_open()) {
        fail(std::format("failed to open {}", path.string()));
    }

    auto result = std::vector<expected_diagnostic>{};
    auto line = std::string{};
    auto line_number = 0uz;
    while(std::getline(stream, line)) {
        ++line_number;
        if(line.empty()) {
            continue;
        }

        auto const fields = split_exact(line, '\t');
        if(fields.size() != 2) {
            fail(std::format(
                "{}:{} expected 2 tab-separated fields, got {}",
                path.string(),
                line_number,
                fields.size()));
        }

        result.push_back(expected_diagnostic{
            .code = [&]() {
                for(auto const code : {
                        parser_diagnostic_code::unexpected_token,
                        parser_diagnostic_code::expected_token,
                        parser_diagnostic_code::expected_identifier,
                        parser_diagnostic_code::expected_expression,
                        parser_diagnostic_code::expected_statement,
                        parser_diagnostic_code::expected_type,
                        parser_diagnostic_code::unsupported_construct,
                        parser_diagnostic_code::lexical_failure,
                    }) {
                    if(to_string(code) == fields[0]) {
                        return code;
                    }
                }

                fail(std::format("unknown parser diagnostic code '{}'", fields[0]));
            }(),
            .span_lexeme = fields[1],
        });
    }

    return result;
}

[[nodiscard]] inline auto parse_expected_status(std::filesystem::path const& path) -> bool
{
    auto const text = read_text(path);
    if(text == "accepted" || text == "accepted\n") {
        return true;
    }
    if(text == "rejected" || text == "rejected\n") {
        return false;
    }

    fail(std::format("unexpected status content in {}", path.string()));
}

[[nodiscard]] inline auto discover_cases(std::filesystem::path const& relative_root)
    -> std::vector<parser_case>
{
    auto const root = cases_root() / relative_root;
    assert_true(std::filesystem::exists(root), std::format("{} should exist", root.string()));

    auto paths = std::vector<std::filesystem::path>{};
    for(auto const& entry : std::filesystem::recursive_directory_iterator{ root }) {
        if(entry.is_regular_file() && entry.path().extension() == ".cp") {
            paths.push_back(entry.path());
        }
    }

    std::ranges::sort(paths);
    assert_true(!paths.empty(), std::format("{} should contain cases", root.string()));

    auto result = std::vector<parser_case>{};
    result.reserve(paths.size());
    for(auto const& source_path : paths) {
        auto const stem = source_path.stem().string();
        result.push_back(parser_case{
            .source_path = source_path,
            .source_text = normalize_source_text(read_text(source_path)),
            .accepted = parse_expected_status(source_path.parent_path() / (stem + ".status")),
            .diagnostics = parse_expected_diagnostics(source_path.parent_path() / (stem + ".diag")),
        });
    }

    return result;
}

} // namespace test_parser
