#pragma once

#include "jsonl.hpp"

namespace test_preprocessor {

struct expected_issue {
    preprocess_issue_kind kind{};
    std::string span_lexeme;
    std::size_t start{};
    std::size_t end{};
    std::size_t line{};
    std::size_t column{};


    auto operator==(expected_issue const&) const -> bool = default;
};

struct preprocessor_case {
    std::filesystem::path source_path;
    std::string source_text;
    std::string normalized_text;
    std::vector<expected_issue> issues;
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

auto inline to_expected_issue(source_manager const& sources, preprocess_issue const& value)
    -> expected_issue
{
    auto const position = sources.position(value.span.start);
    auto const [file, local_start] = sources.locate(value.span.start);
    auto const file_start = sources.file_start(file);
    return expected_issue{
        .kind = value.kind,
        .span_lexeme = std::string{sources.slice(value.span)},
        .start = local_start,
        .end = value.span.end - file_start,
        .line = position.line,
        .column = position.column,
    };
}

auto inline format_issue(expected_issue const& value) -> std::string
{
    return test_support::dump_jsonl_record(test_support::jsonl_record{
        {"kind", std::string{to_string(value.kind)}},
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
