export module lexer.literal;

import std;
import lexer.token;
import diagnostic;
import lexer.charset;

/// @brief 字面量扫描产生的局部诊断。
export struct literal_diagnostic
{
    diagnostic_kind kind{};
    std::string_view message;
    std::size_t start{};
    std::size_t end{};
};

/// @brief 字面量扫描 helper 的结果。
export struct literal_scan_result
{
    token_kind kind{};
    std::size_t end{};
    token_flags flags{};
    std::vector<literal_diagnostic> diagnostics;
};

enum class escape_result
{
    valid,
    invalid,
    unterminated,
};

auto consume_escape_sequence(
    std::string_view source,
    std::size_t& offset,
    std::vector<literal_diagnostic>& diagnostics,
    std::size_t literal_start,
    diagnostic_kind unterminated_kind,
    std::string_view unterminated_message) -> escape_result
{
    using namespace std::literals;

    auto const escape_start = offset;
    ++offset;

    if(offset >= source.size()) {
        diagnostics.emplace_back(unterminated_kind, unterminated_message, literal_start, offset);
        return escape_result::unterminated;
    }

    if(source[offset] == '\n') {
        diagnostics.emplace_back(unterminated_kind, unterminated_message, literal_start, offset);
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
            diagnostics.emplace_back(
                diagnostic_kind::invalid_escape_sequence,
                "invalid escape sequence"sv,
                escape_start,
                offset
            );
            return escape_result::invalid;
        }
        while(offset < source.size() and is_hex_digit(source[offset])) {
            ++offset;
        }
        return escape_result::valid;
    }

    ++offset;
    diagnostics.emplace_back(
        diagnostic_kind::invalid_escape_sequence,
        "invalid escape sequence"sv,
        escape_start,
        offset
    );
    return escape_result::invalid;
}

/// @brief 扫描数字字面量（整数或浮点）。
export auto scan_number_literal(std::string_view source, std::size_t start, token_flags flags) -> literal_scan_result
{
    using namespace std::literals;

    auto result = literal_scan_result{ .flags = flags };
    auto invalid_number = [&](std::size_t end) -> literal_scan_result {
        result.kind = token_kind::invalid;
        result.end = end;
        result.flags = flags | token_flags::recovered;
        result.diagnostics.emplace_back(
            diagnostic_kind::invalid_number_suffix,
            "invalid numeric suffix"sv,
            start,
            end
        );
        return result;
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
            return invalid_number(probe);
        }

        is_float = true;
        offset = probe + 1;
        while(offset < source.size() and is_decimal_digit(source[offset])) {
            ++offset;
        }
    }

    if(offset < source.size() and is_identifier_start(source[offset])) {
        while(offset < source.size() and not is_recovery_delimiter(source[offset])) {
            ++offset;
        }
        return invalid_number(offset);
    }

    if(not is_float and source[start] == '0' and digit_end > start + 1) {
        return invalid_number(digit_end);
    }

    result.kind = is_float ? token_kind::float_literal : token_kind::integer_literal;
    result.end = offset;
    return result;
}

/// @brief 扫描字符串字面量。
export auto scan_string_literal(std::string_view source, std::size_t start, token_flags flags) -> literal_scan_result
{
    using namespace std::literals;

    auto constexpr unterminated_message = "unterminated string literal"sv;
    auto result = literal_scan_result{ .kind = token_kind::string_literal, .flags = flags };
    auto offset = start + 1;
    auto has_invalid_escape = false;

    while(offset < source.size()) {
        auto const ch = source[offset];
        if(ch == '"') {
            ++offset;
            result.end = offset;
            if(has_invalid_escape) {
                result.kind = token_kind::invalid;
                result.flags = flags | token_flags::recovered;
            }
            return result;
        }
        if(ch == '\n') {
            result.diagnostics.emplace_back(
                diagnostic_kind::unterminated_string_literal,
                unterminated_message,
                start,
                offset
            );
            result.kind = token_kind::invalid;
            result.end = offset;
            result.flags = flags | token_flags::unterminated | token_flags::recovered;
            return result;
        }
        if(ch == '\\') {
            auto const escape = consume_escape_sequence(source, offset, result.diagnostics, start,
                diagnostic_kind::unterminated_string_literal, unterminated_message);
            if(escape == escape_result::unterminated) {
                result.kind = token_kind::invalid;
                result.end = offset;
                result.flags = flags | token_flags::unterminated | token_flags::recovered;
                return result;
            }
            if(escape == escape_result::invalid) {
                has_invalid_escape = true;
            }
            continue;
        }
        ++offset;
    }

    result.diagnostics.emplace_back(
        diagnostic_kind::unterminated_string_literal,
        unterminated_message,
        start,
        offset
    );
    result.kind = token_kind::invalid;
    result.end = offset;
    result.flags = flags | token_flags::unterminated | token_flags::recovered;
    return result;
}

/// @brief 扫描字符字面量。
export auto scan_char_literal(std::string_view source, std::size_t start, token_flags flags) -> literal_scan_result
{
    using namespace std::literals;

    auto constexpr unterminated_message = "unterminated char literal"sv;
    auto result = literal_scan_result{ .kind = token_kind::char_literal, .flags = flags };
    auto offset = start + 1;
    auto content_count = 0U;
    auto has_invalid_escape = false;

    while(offset < source.size()) {
        auto const ch = source[offset];
        if(ch == '\'') {
            ++offset;
            result.end = offset;
            if(has_invalid_escape) {
                result.kind = token_kind::invalid;
                result.flags = flags | token_flags::recovered;
                return result;
            }
            if(content_count == 1) {
                return result;
            }
            result.diagnostics.emplace_back(
                diagnostic_kind::invalid_char_literal,
                "invalid char literal"sv,
                start,
                offset
            );
            result.kind = token_kind::invalid;
            result.flags = flags | token_flags::recovered;
            return result;
        }
        if(ch == '\n') {
            result.diagnostics.emplace_back(
                diagnostic_kind::unterminated_char_literal,
                unterminated_message,
                start,
                offset
            );
            result.kind = token_kind::invalid;
            result.end = offset;
            result.flags = flags | token_flags::unterminated | token_flags::recovered;
            return result;
        }
        if(ch == '\\') {
            auto const escape = consume_escape_sequence(source, offset, result.diagnostics, start,
                diagnostic_kind::unterminated_char_literal, unterminated_message);
            if(escape == escape_result::unterminated) {
                result.kind = token_kind::invalid;
                result.end = offset;
                result.flags = flags | token_flags::unterminated | token_flags::recovered;
                return result;
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

    result.diagnostics.emplace_back(
        diagnostic_kind::unterminated_char_literal,
        unterminated_message,
        start,
        offset
    );
    result.kind = token_kind::invalid;
    result.end = offset;
    result.flags = flags | token_flags::unterminated | token_flags::recovered;
    return result;
}
