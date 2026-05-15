#pragma once

#include "jsonl.hpp"

namespace test_preprocessor {

struct expected_diagnostic {
    diagnostic_kind kind{};
    std::string span_lexeme;
    std::size_t start{};
    std::size_t end{};
    std::size_t line{};
    std::size_t column{};


    auto operator==(expected_diagnostic const&) const -> bool = default;
};

struct preprocessor_case {
    std::filesystem::path source_path;
    std::string source_text;
    std::string normalized_text;
    std::vector<expected_diagnostic> diagnostics;
};

auto inline escape_field(std::string_view text) -> std::string
{
    auto result = std::string{};
    result.reserve(text.size());

    for (auto const ch : text) {
        switch (ch) {
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result.push_back(ch);
            break;
        }
    }

    return result;
}

auto inline to_expected_diagnostic(source_manager const& sources, diagnostic const& value)
    -> expected_diagnostic
{
    auto const position = sources.position(value.primary_span.start);
    auto const [file, local_start] = sources.locate(value.primary_span.start);
    auto const file_start = sources.file_start(file);
    return expected_diagnostic{
        .kind = value.kind,
        .span_lexeme = std::string{sources.slice(value.primary_span)},
        .start = local_start,
        .end = value.primary_span.end - file_start,
        .line = position.line,
        .column = position.column,
    };
}

auto inline format_diagnostic(expected_diagnostic const& value) -> std::string
{
    return test_support::dump_jsonl_record(test_support::jsonl_record{
        {"code", std::string{spec(value.kind).code}},
        {"span", value.span_lexeme},
        {"start", value.start},
        {"end", value.end},
        {"line", value.line},
        {"column", value.column},
    });
}

auto inline join_lines(std::vector<std::string> const& lines) -> std::string
{
    auto result = std::string{};
    for (auto const& line : lines) {
        result += line;
        result += '\n';
    }
    return result;
}

} // namespace test_preprocessor
