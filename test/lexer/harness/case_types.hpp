#pragma once

namespace test_lexer {

using namespace std::literals;

struct expected_token {
    front::token_kind kind{};
    std::string lexeme;
    front::token_flags flags{front::token_flags::none};

    [[nodiscard]] auto operator==(expected_token const&) const -> bool = default;
};

struct expected_diagnostic {
    front::diagnostic_code code{};
    std::string span_lexeme;

    [[nodiscard]] auto operator==(expected_diagnostic const&) const -> bool = default;
};

struct lexer_case {
    std::filesystem::path source_path;
    std::string source_text;
    std::vector<expected_token> tokens;
    std::vector<expected_diagnostic> diagnostics;
};

inline constexpr auto all_token_kinds = std::array{
    front::token_kind::eof,
    front::token_kind::invalid,
    front::token_kind::identifier,
    front::token_kind::integer_literal,
    front::token_kind::float_literal,
    front::token_kind::char_literal,
    front::token_kind::string_literal,
    front::token_kind::kw_let,
    front::token_kind::kw_const,
    front::token_kind::kw_if,
    front::token_kind::kw_else,
    front::token_kind::kw_while,
    front::token_kind::kw_do,
    front::token_kind::kw_for,
    front::token_kind::kw_break,
    front::token_kind::kw_continue,
    front::token_kind::kw_return,
    front::token_kind::kw_import,
    front::token_kind::kw_export,
    front::token_kind::kw_module,
    front::token_kind::kw_struct,
    front::token_kind::kw_impl,
    front::token_kind::kw_trait,
    front::token_kind::kw_as,
    front::token_kind::kw_true,
    front::token_kind::kw_false,
    front::token_kind::kw_and,
    front::token_kind::kw_or,
    front::token_kind::kw_not,
    front::token_kind::l_paren,
    front::token_kind::r_paren,
    front::token_kind::l_brace,
    front::token_kind::r_brace,
    front::token_kind::l_bracket,
    front::token_kind::r_bracket,
    front::token_kind::comma,
    front::token_kind::semicolon,
    front::token_kind::colon,
    front::token_kind::colon_colon,
    front::token_kind::dot,
    front::token_kind::arrow,
    front::token_kind::plus,
    front::token_kind::plus_equal,
    front::token_kind::minus,
    front::token_kind::minus_equal,
    front::token_kind::star,
    front::token_kind::star_equal,
    front::token_kind::slash,
    front::token_kind::slash_equal,
    front::token_kind::percent,
    front::token_kind::percent_equal,
    front::token_kind::equal,
    front::token_kind::equal_equal,
    front::token_kind::bang_equal,
    front::token_kind::less,
    front::token_kind::less_equal,
    front::token_kind::greater,
    front::token_kind::greater_equal,
    front::token_kind::amp,
    front::token_kind::amp_equal,
    front::token_kind::pipe,
    front::token_kind::pipe_equal,
    front::token_kind::caret,
    front::token_kind::caret_equal,
    front::token_kind::tilde,
    front::token_kind::less_less,
    front::token_kind::less_less_equal,
    front::token_kind::greater_greater,
    front::token_kind::greater_greater_equal,
    front::token_kind::plus_plus,
    front::token_kind::minus_minus,
    front::token_kind::question,
};

inline constexpr auto all_diagnostic_codes = std::array{
    front::diagnostic_code::invalid_character,
    front::diagnostic_code::unterminated_string_literal,
    front::diagnostic_code::unterminated_char_literal,
    front::diagnostic_code::unterminated_block_comment,
    front::diagnostic_code::invalid_char_literal,
    front::diagnostic_code::invalid_escape_sequence,
    front::diagnostic_code::invalid_number_suffix,
};

inline auto format_flags(front::token_flags flags) -> std::string
{
    auto names = std::vector<std::string_view>{};
    if (front::has_flag(flags, front::token_flags::leading_space)) {
        names.push_back("leading_space");
    }
    if (front::has_flag(flags, front::token_flags::start_of_line)) {
        names.push_back("start_of_line");
    }
    if (front::has_flag(flags, front::token_flags::unterminated)) {
        names.push_back("unterminated");
    }
    if (front::has_flag(flags, front::token_flags::recovered)) {
        names.push_back("recovered");
    }

    if (names.empty()) {
        return "-";
    }

    auto result = std::string{};
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (index != 0) {
            result += ',';
        }
        result += names[index];
    }
    return result;
}

inline auto to_expected_token(front::source_manager const& sources, front::token const& value)
    -> expected_token
{
    return expected_token{
        .kind = value.kind,
        .lexeme = std::string(sources.slice(value.source_span)),
        .flags = value.flags,
    };
}

inline auto to_expected_diagnostic(
    front::source_manager const& sources,
    front::diagnostic const& value) -> expected_diagnostic
{
    return expected_diagnostic{
        .code = value.code,
        .span_lexeme = std::string(sources.slice(value.primary_span)),
    };
}

inline auto format_token(expected_token const& value) -> std::string
{
    return std::format(
        "{}\t{}\t{}",
        front::to_string(value.kind),
        value.lexeme,
        format_flags(value.flags));
}

inline auto diagnostic_code_name(front::diagnostic_code code) -> std::string_view
{
    using enum front::diagnostic_code;

    switch (code) {
    case invalid_character: return "invalid_character";
    case unterminated_string_literal: return "unterminated_string_literal";
    case unterminated_char_literal: return "unterminated_char_literal";
    case unterminated_block_comment: return "unterminated_block_comment";
    case invalid_char_literal: return "invalid_char_literal";
    case invalid_escape_sequence: return "invalid_escape_sequence";
    case invalid_number_suffix: return "invalid_number_suffix";
    }

    return "unknown_diagnostic";
}

inline auto format_diagnostic(expected_diagnostic const& value) -> std::string
{
    return std::format("{}\t{}", diagnostic_code_name(value.code), value.span_lexeme);
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

} // namespace test_lexer
