#pragma once

#include "jsonl.hpp"

namespace test_parser {

struct expected_diagnostic
{
    parser_diagnostic_code code{};
    std::string span_lexeme;
    std::size_t start{};
    std::size_t end{};
    std::size_t line{};
    std::size_t column{};

    [[nodiscard]] auto operator==(expected_diagnostic const&) const -> bool = default;
};

struct parser_case
{
    std::filesystem::path source_path;
    std::string source_text;
    bool accepted{};
    std::vector<expected_diagnostic> diagnostics;
};

inline auto to_expected_diagnostic(
    source_manager const& sources,
    parser_diagnostic const& value) -> expected_diagnostic
{
    auto const position = sources.position(value.primary_span.file, value.primary_span.start);
    return expected_diagnostic{
        .code = value.code,
        .span_lexeme = std::string(sources.slice(value.primary_span)),
        .start = value.primary_span.start,
        .end = value.primary_span.end,
        .line = position.line,
        .column = position.column,
    };
}

inline auto format_diagnostic(expected_diagnostic const& value) -> std::string
{
    return test_support::dump_jsonl_record(test_support::jsonl_record{
        {"code", std::string(to_string(value.code))},
        {"span", value.span_lexeme},
        {"start", value.start},
        {"end", value.end},
        {"line", value.line},
        {"column", value.column},
    });
}

inline auto join_lines(std::vector<std::string> const& lines) -> std::string
{
    auto result = std::string{};
    for(auto const& line : lines) {
        result += line;
        result += '\n';
    }
    return result;
}

} // namespace test_parser
