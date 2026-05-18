#pragma once

#include "jsonl.hpp"

namespace test_lexer {

using namespace std::literals;

struct expected_token {
    token_kind kind{};
    std::string lexeme;
    std::size_t start{};
    std::size_t end{};
    std::size_t line{};
    std::size_t column{};
    token_flags flags{token_flags::none};

    [[nodiscard]]
    auto operator==(expected_token const&) const -> bool = default;
};

struct expected_diagnostic {
    diagnostic_kind kind{};
    std::string span_lexeme;
    std::size_t start{};
    std::size_t end{};
    std::size_t line{};
    std::size_t column{};

    [[nodiscard]]
    auto operator==(expected_diagnostic const&) const -> bool = default;
};

struct lexer_case {
    std::filesystem::path source_path;
    std::string source_text;
    std::vector<expected_token> tokens;
    std::vector<expected_diagnostic> diagnostics;
};

auto constexpr inline all_token_kinds = std::array {
    token_kind::eof,
    token_kind::invalid,
    token_kind::identifier,
    token_kind::integer_literal,
    token_kind::float_literal,
    token_kind::char_literal,
    token_kind::string_literal,
    token_kind::kw_let,
    token_kind::kw_const,
    token_kind::kw_if,
    token_kind::kw_else,
    token_kind::kw_while,
    token_kind::kw_do,
    token_kind::kw_for,
    token_kind::kw_break,
    token_kind::kw_continue,
    token_kind::kw_return,
    token_kind::kw_import,
    token_kind::kw_export,
    token_kind::kw_module,
    token_kind::kw_struct,
    token_kind::kw_impl,
    token_kind::kw_concept,
    token_kind::kw_as,
    token_kind::kw_true,
    token_kind::kw_false,
    token_kind::kw_and,
    token_kind::kw_or,
    token_kind::kw_not,
    token_kind::l_paren,
    token_kind::r_paren,
    token_kind::l_brace,
    token_kind::r_brace,
    token_kind::l_bracket,
    token_kind::r_bracket,
    token_kind::comma,
    token_kind::semicolon,
    token_kind::colon,
    token_kind::colon_colon,
    token_kind::dot,
    token_kind::arrow,
    token_kind::plus,
    token_kind::plus_equal,
    token_kind::minus,
    token_kind::minus_equal,
    token_kind::star,
    token_kind::star_equal,
    token_kind::slash,
    token_kind::slash_equal,
    token_kind::percent,
    token_kind::percent_equal,
    token_kind::equal,
    token_kind::equal_equal,
    token_kind::bang_equal,
    token_kind::less,
    token_kind::less_equal,
    token_kind::greater,
    token_kind::greater_equal,
    token_kind::amp,
    token_kind::amp_equal,
    token_kind::pipe,
    token_kind::pipe_equal,
    token_kind::caret,
    token_kind::caret_equal,
    token_kind::tilde,
    token_kind::less_less,
    token_kind::less_less_equal,
    token_kind::greater_greater,
    token_kind::greater_greater_equal,
    token_kind::plus_plus,
    token_kind::minus_minus,
    token_kind::question,
};

auto constexpr inline all_diagnostic_kinds = std::array {
    diagnostic_kind::invalid_character,
    diagnostic_kind::unterminated_string_literal,
    diagnostic_kind::unterminated_char_literal,
    diagnostic_kind::invalid_char_literal,
    diagnostic_kind::invalid_escape_sequence,
    diagnostic_kind::invalid_number_suffix,
};

auto inline format_flags(token_flags flags) -> std::string
{
    auto names = std::vector<std::string_view>{};
    if (has_flag(flags, token_flags::leading_space)) {
        names.push_back("leading_space");
    }
    if (has_flag(flags, token_flags::start_of_line)) {
        names.push_back("start_of_line");
    }
    if (has_flag(flags, token_flags::unterminated)) {
        names.push_back("unterminated");
    }
    if (has_flag(flags, token_flags::recovered)) {
        names.push_back("recovered");
    }

    if (names.empty()) {
        return "-";
    }

    auto result = std::string{names.front()};
    std::ranges::for_each (
        names | std::views::drop(1),
        [&](auto const name) {
            result += ',';
            result += name;
        }
    );
    return result;
}

auto inline to_expected_token(source_manager const& sources, token const& value)
    -> expected_token
{
    auto const position = sources.position(value.span.start);
    auto const [file, local_start] = sources.locate(value.span.start);
    auto const file_start = sources.file_start(file);
    return expected_token{
        .kind = value.kind,
        .lexeme = std::string{sources.slice(value.span)},
        .start = local_start,
        .end = value.span.end - file_start,
        .line = position.line,
        .column = position.column,
        .flags = value.flags,
    };
}

auto inline to_expected_diagnostic(
    source_manager const& sources,
    diagnostic const& value) -> expected_diagnostic
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

auto inline format_token(expected_token const& value) -> std::string
{
    return test_support::dump_jsonl_record(test_support::jsonl_record{
        {"kind", std::string{to_string(value.kind)}},
        {"lexeme", value.lexeme},
        {"start", value.start},
        {"end", value.end},
        {"line", value.line},
        {"column", value.column},
        {"flags", format_flags(value.flags)},
    });
}

auto inline diagnostic_kind_name(diagnostic_kind kind) -> std::string_view
{
    return spec(kind).code;
}

auto inline format_diagnostic(expected_diagnostic const& value) -> std::string
{
    return test_support::dump_jsonl_record(test_support::jsonl_record{
        {"code", std::string{diagnostic_kind_name(value.kind)}},
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

} // namespace test_lexer
