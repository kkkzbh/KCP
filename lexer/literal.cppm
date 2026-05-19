module lexer:literal;

import std;
import diagnostic;
import lexer.charset;
import lexer.cursor;
import lexer.token;
import :state;

enum class escape_result
{
    valid,
    invalid,
    unterminated,
};

auto consume_escape_sequence(std::string_view source, std::size_t& offset, diagnostic_collector& diagnostics, lexer_cursor const& cursor, std::size_t literal_start, diagnostic_kind unterminated_kind, std::string_view unterminated_message) -> escape_result
{
    using namespace std::literals;

    auto const escape_start = offset;
    ++offset;

    if(offset >= source.size()) {
        diagnostics.report(
            unterminated_kind,
            unterminated_message,
            cursor.make_span(literal_start, offset)
        );
        return escape_result::unterminated;
    }

    if(source[offset] == '\n') {
        diagnostics.report(
            unterminated_kind,
            unterminated_message,
            cursor.make_span(literal_start, offset)
        );
        return escape_result::unterminated;
    }

    if(is_simple_escape(source[offset])) {
        ++offset;
        return escape_result::valid;
    }

    if(is_octal_digit(source[offset])) {
        ++offset;
        for(auto extra = 0; extra < 2 and offset < source.size() and is_octal_digit(source[offset]); ++extra) {
            ++offset;
        }
        return escape_result::valid;
    }

    if(source[offset] == 'x') {
        ++offset;
        if(offset >= source.size() or not is_hex_digit(source[offset])) {
            diagnostics.report(
                diagnostic_kind::invalid_escape_sequence,
                "invalid escape sequence"sv,
                cursor.make_span(escape_start, offset)
            );
            return escape_result::invalid;
        }
        while(offset < source.size() and is_hex_digit(source[offset])) {
            ++offset;
        }
        return escape_result::valid;
    }

    ++offset;
    diagnostics.report(
        diagnostic_kind::invalid_escape_sequence,
        "invalid escape sequence"sv,
        cursor.make_span(escape_start, offset)
    );
    return escape_result::invalid;
}

auto lexer::lex_number_literal(token_flags flags) -> token
{
    auto const source = cursor_.source();
    auto const start = cursor_.offset();
    auto invalid_number = [&](diagnostic_kind kind, std::size_t diagnostic_start, std::size_t diagnostic_end, std::size_t token_end) -> token {
        cursor_.set_offset(token_end);
        diagnostics_.report(kind, cursor_.make_span(diagnostic_start, diagnostic_end));
        return cursor_.make_token(token_kind::invalid, start, cursor_.offset(), flags | token_flags::recovered);
    };

    auto offset = start;
    while(offset < source.size() and is_decimal_digit(source[offset])) {
        ++offset;
    }
    auto const digit_end = offset;

    auto is_float = (
        offset < source.size() and source[offset] == '.'
        and (offset + 1 >= source.size() or not is_identifier_start(source[offset + 1]))
    );
    if(is_float) {
        ++offset;
        while(offset < source.size() and is_decimal_digit(source[offset])) {
            ++offset;
        }
    }

    if(offset < source.size() and is_exponent_marker(source[offset])) {
        auto probe = offset + 1;
        if(probe < source.size() and (source[probe] == '+' or source[probe] == '-')) {
            ++probe;
        }
        if(probe >= source.size() or not is_decimal_digit(source[probe])) {
            return invalid_number(diagnostic_kind::missing_exponent_digits, offset, probe, probe);
        }

        is_float = true;
        offset = probe + 1;
        while(offset < source.size() and is_decimal_digit(source[offset])) {
            ++offset;
        }
    }

    if(offset < source.size() and is_identifier_start(source[offset])) {
        auto const suffix_start = offset;
        while(offset < source.size() and not is_recovery_delimiter(source[offset])) {
            ++offset;
        }
        return invalid_number(diagnostic_kind::invalid_number_suffix, suffix_start, offset, offset);
    }

    if(not is_float and source[start] == '0' and digit_end > start + 1) {
        return invalid_number(diagnostic_kind::leading_zero_integer, start, digit_end, digit_end);
    }

    cursor_.set_offset(offset);
    auto const kind = is_float ? token_kind::float_literal : token_kind::integer_literal;
    return cursor_.make_token(kind, start, cursor_.offset(), flags);
}

auto lexer::lex_string_literal(token_flags flags) -> token
{
    using namespace std::literals;

    auto const source = cursor_.source();
    auto const start = cursor_.offset();
    auto constexpr unterminated_message = "unterminated string literal"sv;
    auto offset = start + 1;
    auto has_invalid_escape = false;

    auto finish = [&](token_kind kind, std::size_t end, token_flags result_flags) -> token {
        cursor_.set_offset(end);
        return cursor_.make_token(kind, start, cursor_.offset(), result_flags);
    };

    while(offset < source.size()) {
        auto const ch = source[offset];
        if(ch == '"') {
            ++offset;
            if(has_invalid_escape) {
                return finish(token_kind::invalid, offset, flags | token_flags::recovered);
            }
            return finish(token_kind::string_literal, offset, flags);
        }
        if(ch == '\n') {
            diagnostics_.report(
                diagnostic_kind::unterminated_string_literal,
                unterminated_message,
                cursor_.make_span(start, offset)
            );
            return finish(token_kind::invalid, offset, flags | token_flags::unterminated | token_flags::recovered);
        }
        if(ch == '\\') {
            auto const escape = consume_escape_sequence(
                source,
                offset,
                diagnostics_,
                cursor_,
                start,
                diagnostic_kind::unterminated_string_literal,
                unterminated_message
            );
            if(escape == escape_result::unterminated) {
                return finish(token_kind::invalid, offset, flags | token_flags::unterminated | token_flags::recovered);
            }
            if(escape == escape_result::invalid) {
                has_invalid_escape = true;
            }
            continue;
        }
        ++offset;
    }

    diagnostics_.report(
        diagnostic_kind::unterminated_string_literal,
        unterminated_message,
        cursor_.make_span(start, offset)
    );
    return finish(token_kind::invalid, offset, flags | token_flags::unterminated | token_flags::recovered);
}

auto lexer::lex_char_literal(token_flags flags) -> token
{
    using namespace std::literals;

    auto const source = cursor_.source();
    auto const start = cursor_.offset();
    auto constexpr unterminated_message = "unterminated char literal"sv;
    auto offset = start + 1;
    auto content_count = 0U;
    auto has_invalid_escape = false;

    auto finish = [&](token_kind kind, std::size_t end, token_flags result_flags) -> token {
        cursor_.set_offset(end);
        return cursor_.make_token(kind, start, cursor_.offset(), result_flags);
    };

    while(offset < source.size()) {
        auto const ch = source[offset];
        if(ch == '\'') {
            ++offset;
            if(has_invalid_escape) {
                return finish(token_kind::invalid, offset, flags | token_flags::recovered);
            }
            if(content_count == 1) {
                return finish(token_kind::char_literal, offset, flags);
            }
            diagnostics_.report(
                diagnostic_kind::invalid_char_literal,
                "invalid char literal"sv,
                cursor_.make_span(start, offset)
            );
            return finish(token_kind::invalid, offset, flags | token_flags::recovered);
        }
        if(ch == '\n') {
            diagnostics_.report(
                diagnostic_kind::unterminated_char_literal,
                unterminated_message,
                cursor_.make_span(start, offset)
            );
            return finish(token_kind::invalid, offset, flags | token_flags::unterminated | token_flags::recovered);
        }
        if(ch == '\\') {
            auto const escape = consume_escape_sequence(
                source,
                offset,
                diagnostics_,
                cursor_,
                start,
                diagnostic_kind::unterminated_char_literal,
                unterminated_message
            );
            if(escape == escape_result::unterminated) {
                return finish(token_kind::invalid, offset, flags | token_flags::unterminated | token_flags::recovered);
            }
            if(escape == escape_result::invalid) {
                has_invalid_escape = true;
            }
            ++content_count;
            continue;
        }
        ++content_count;
        ++offset;
    }

    diagnostics_.report(
        diagnostic_kind::unterminated_char_literal,
        unterminated_message,
        cursor_.make_span(start, offset)
    );
    return finish(token_kind::invalid, offset, flags | token_flags::unterminated | token_flags::recovered);
}
