#pragma once

namespace test_parser {

struct expected_diagnostic
{
    parser_diagnostic_code code{};
    std::string span_lexeme;

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
    return expected_diagnostic{
        .code = value.code,
        .span_lexeme = std::string(sources.slice(value.primary_span)),
    };
}

inline auto format_diagnostic(expected_diagnostic const& value) -> std::string
{
    return std::format("{}\t{}", to_string(value.code), value.span_lexeme);
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
