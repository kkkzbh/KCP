#pragma once

namespace test_preprocessor {

struct expected_issue {
    preprocess_issue_kind kind{};
    std::string span_lexeme;

    [[nodiscard]] auto operator==(expected_issue const&) const -> bool = default;
};

struct preprocessor_case {
    std::filesystem::path source_path;
    std::string source_text;
    std::string normalized_text;
    std::vector<expected_issue> issues;
};

inline auto issue_kind_name(preprocess_issue_kind kind) -> std::string_view
{
    switch (kind) {
    case preprocess_issue_kind::unterminated_block_comment:
        return "unterminated_block_comment";
    }

    return "unknown_issue_kind";
}

inline auto escape_field(std::string_view text) -> std::string
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

inline auto to_expected_issue(source_manager const& sources, preprocess_issue const& value)
    -> expected_issue
{
    return expected_issue{
        .kind = value.kind,
        .span_lexeme = std::string(sources.slice(value.source_span)),
    };
}

inline auto format_issue(expected_issue const& value) -> std::string
{
    return std::format(
        "{}\t{}",
        issue_kind_name(value.kind),
        escape_field(value.span_lexeme));
}

inline auto join_lines(std::vector<std::string> const& lines) -> std::string
{
    auto result = std::string{};
    for (auto const& line : lines) {
        result += line;
        result += '\n';
    }
    return result;
}

} // namespace test_preprocessor
