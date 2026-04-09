export module lexer.scanner;

import std;
import lexer.source;
import lexer.token;
import lexer.diagnostic;

export struct lexer
{
    lexer(source_manager const& sources, file_id file, diagnostic_sink& sink);

    auto reset(file_id file) -> void;

    [[nodiscard]] auto peek() -> token;

    auto next() -> token;

    [[nodiscard]] auto tokenize_all() -> std::vector<token>;

private:
    enum class escape_result
    {
        valid,
        invalid,
        unterminated,
    };

    struct trivia_state
    {
        bool saw_space{};
        bool at_line_start{ true };
    };

    [[nodiscard]] auto current_source() const -> std::string_view;

    [[nodiscard]] auto eof() const -> bool;

    [[nodiscard]] auto current() const -> char;

    [[nodiscard]] auto peek_char() const -> char;

    auto advance(std::size_t count = 1) -> void;

    [[nodiscard]] auto make_token(token_kind kind, std::size_t start, std::size_t end,
                                  token_flags flags) const -> token;

    auto report(diagnostic_code code, std::string const& message, std::size_t start, std::size_t end) const
        -> void;

    [[nodiscard]] auto skip_trivia() -> trivia_state;

    [[nodiscard]] auto lex_token(token_flags flags) -> token;

    [[nodiscard]] auto lex_identifier_or_keyword(std::size_t start, token_flags flags) -> token;

    [[nodiscard]] auto lex_number(std::size_t start, token_flags flags) -> token;

    [[nodiscard]] auto lex_string_literal(std::size_t start, token_flags flags) -> token;

    [[nodiscard]] auto lex_char_literal(std::size_t start, token_flags flags) -> token;

    [[nodiscard]] auto consume_escape_sequence(
        std::size_t literal_start,
        diagnostic_code unterminated_code) -> escape_result;

    [[nodiscard]] auto lex_invalid_after_number(std::size_t start, token_flags flags) -> token;

    [[nodiscard]] auto lex_invalid_character(std::size_t start, token_flags flags) -> token;

    [[nodiscard]] auto lex_unterminated_block_comment(std::size_t start, token_flags flags) -> token;

    source_manager const& sources_;
    diagnostic_sink& sink_;
    file_id file_{};
    std::size_t offset_{};
    bool has_peeked_{};
    token peeked_{};
    bool at_line_start_{ true };
};

namespace detail {
using namespace std::literals;

constexpr auto punctuation_table = std::to_array({
    std::pair{ "<<="sv, token_kind::less_less_equal },
    std::pair{ ">>="sv, token_kind::greater_greater_equal },
    std::pair{ "::"sv, token_kind::colon_colon },
    std::pair{ "->"sv, token_kind::arrow },
    std::pair{ "=="sv, token_kind::equal_equal },
    std::pair{ "!="sv, token_kind::bang_equal },
    std::pair{ "<="sv, token_kind::less_equal },
    std::pair{ ">="sv, token_kind::greater_equal },
    std::pair{ "<<"sv, token_kind::less_less },
    std::pair{ ">>"sv, token_kind::greater_greater },
    std::pair{ "++"sv, token_kind::plus_plus },
    std::pair{ "--"sv, token_kind::minus_minus },
    std::pair{ "+="sv, token_kind::plus_equal },
    std::pair{ "-="sv, token_kind::minus_equal },
    std::pair{ "*="sv, token_kind::star_equal },
    std::pair{ "/="sv, token_kind::slash_equal },
    std::pair{ "%="sv, token_kind::percent_equal },
    std::pair{ "&="sv, token_kind::amp_equal },
    std::pair{ "|="sv, token_kind::pipe_equal },
    std::pair{ "^="sv, token_kind::caret_equal },
    std::pair{ "("sv, token_kind::l_paren },
    std::pair{ ")"sv, token_kind::r_paren },
    std::pair{ "{"sv, token_kind::l_brace },
    std::pair{ "}"sv, token_kind::r_brace },
    std::pair{ "["sv, token_kind::l_bracket },
    std::pair{ "]"sv, token_kind::r_bracket },
    std::pair{ ","sv, token_kind::comma },
    std::pair{ ";"sv, token_kind::semicolon },
    std::pair{ ":"sv, token_kind::colon },
    std::pair{ "."sv, token_kind::dot },
    std::pair{ "+"sv, token_kind::plus },
    std::pair{ "-"sv, token_kind::minus },
    std::pair{ "*"sv, token_kind::star },
    std::pair{ "/"sv, token_kind::slash },
    std::pair{ "%"sv, token_kind::percent },
    std::pair{ "="sv, token_kind::equal },
    std::pair{ "<"sv, token_kind::less },
    std::pair{ ">"sv, token_kind::greater },
    std::pair{ "&"sv, token_kind::amp },
    std::pair{ "|"sv, token_kind::pipe },
    std::pair{ "^"sv, token_kind::caret },
    std::pair{ "~"sv, token_kind::tilde },
    std::pair{ "?"sv, token_kind::question },
});

constexpr auto keyword_table = std::to_array({
    std::pair{ "let"sv, token_kind::kw_let },
    std::pair{ "const"sv, token_kind::kw_const },
    std::pair{ "if"sv, token_kind::kw_if },
    std::pair{ "else"sv, token_kind::kw_else },
    std::pair{ "while"sv, token_kind::kw_while },
    std::pair{ "do"sv, token_kind::kw_do },
    std::pair{ "for"sv, token_kind::kw_for },
    std::pair{ "break"sv, token_kind::kw_break },
    std::pair{ "continue"sv, token_kind::kw_continue },
    std::pair{ "return"sv, token_kind::kw_return },
    std::pair{ "import"sv, token_kind::kw_import },
    std::pair{ "export"sv, token_kind::kw_export },
    std::pair{ "module"sv, token_kind::kw_module },
    std::pair{ "struct"sv, token_kind::kw_struct },
    std::pair{ "impl"sv, token_kind::kw_impl },
    std::pair{ "trait"sv, token_kind::kw_trait },
    std::pair{ "as"sv, token_kind::kw_as },
    std::pair{ "true"sv, token_kind::kw_true },
    std::pair{ "false"sv, token_kind::kw_false },
    std::pair{ "and"sv, token_kind::kw_and },
    std::pair{ "or"sv, token_kind::kw_or },
    std::pair{ "not"sv, token_kind::kw_not },
});

[[nodiscard]] auto is_identifier_start(char ch) -> bool
{
    return ch == '_' || std::isalpha(static_cast<unsigned char>(ch)) != 0;
}

[[nodiscard]] auto is_identifier_continue(char ch) -> bool
{
    return ch == '_' || std::isalnum(static_cast<unsigned char>(ch)) != 0;
}

[[nodiscard]] auto is_recovery_delimiter(char ch) -> bool
{
    constexpr auto delimiters = " \t\r\n(){}[],:;.+-*/%<>=&|^~?\"'"sv;
    return delimiters.contains(ch);
}

[[nodiscard]] auto is_simple_escape(char ch) -> bool
{
    constexpr auto escapes = "\\'\"?abfnrtv"sv;
    return std::ranges::find(escapes, ch) != escapes.end();
}

[[nodiscard]] auto is_octal_digit(char ch) -> bool
{
    return ch >= '0' && ch <= '7';
}

[[nodiscard]] auto is_hex_digit(char ch) -> bool
{
    return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}

[[nodiscard]] auto keyword_kind(std::string_view text) -> std::optional<token_kind>
{
    for(auto const& [keyword, kind] : keyword_table) {
        if(keyword == text) {
            return kind;
        }
    }

    return std::nullopt;
}
} // namespace detail

lexer::lexer(source_manager const& sources, file_id file, diagnostic_sink& sink) : sources_(sources), sink_(sink),
    file_(file) {}

auto lexer::reset(file_id file) -> void
{
    file_ = file;
    offset_ = 0;
    has_peeked_ = false;
    peeked_ = {};
    at_line_start_ = true;
}

auto lexer::peek() -> token
{
    if(!has_peeked_) {
        peeked_ = next();
        has_peeked_ = true;
    }

    return peeked_;
}

auto lexer::next() -> token
{
    if(has_peeked_) {
        has_peeked_ = false;
        return peeked_;
    }

    auto const trivia = skip_trivia();
    auto flags = token_flags::none;
    if(trivia.saw_space) {
        flags |= token_flags::leading_space;
    }
    if(trivia.at_line_start) {
        flags |= token_flags::start_of_line;
    }

    if(eof()) {
        return make_token(token_kind::eof, offset_, offset_, flags);
    }

    return lex_token(flags);
}

auto lexer::tokenize_all() -> std::vector<token>
{
    auto result = std::vector<token>{};
    for(;;) {
        auto const current_token = next();
        result.push_back(current_token);
        if(current_token.kind == token_kind::eof) {
            break;
        }
    }

    return result;
}

auto lexer::current_source() const -> std::string_view
{
    return sources_.text(file_);
}

auto lexer::eof() const -> bool
{
    return offset_ >= current_source().size();
}

auto lexer::current() const -> char
{
    return eof() ? '\0' : current_source()[offset_];
}

auto lexer::peek_char() const -> char
{
    auto const index = offset_ + 1;
    return index < current_source().size() ? current_source()[index] : '\0';
}

auto lexer::advance(std::size_t count) -> void
{
    offset_ += count;
}

auto lexer::make_token(token_kind kind, std::size_t start, std::size_t end, token_flags flags) const
    -> token
{
    return token{
        .kind = kind,
        .source_span = span{ .file = file_,.start = start,.end = end },
        .flags = flags,
    };
}

auto lexer::report(diagnostic_code code, std::string const& message, std::size_t start, std::size_t end) const
    -> void
{
    sink_.report(diagnostic{
        .severity = diagnostic_severity::error,
        .code = code,
        .message = message,
        .primary_span = span{ .file = file_,.start = start,.end = end },
    });
}

auto lexer::skip_trivia() -> trivia_state
{
    auto result = trivia_state{ .saw_space = false,.at_line_start = at_line_start_ };

    while(!eof()) {
        auto const ch = current();

        if(std::isspace(static_cast<unsigned char>(ch)) != 0) {
            result.saw_space = true;
            if(ch == '\n') {
                at_line_start_ = true;
                result.at_line_start = true;
            }
            advance();
            continue;
        }

        if(ch == '/' && peek_char() == '/') {
            result.saw_space = true;
            advance(2);
            while(!eof() && current() != '\n') {
                advance();
            }
            continue;
        }

        if(ch == '/' && peek_char() == '*') {
            result.saw_space = true;
            auto const start = offset_;
            advance(2);
            auto closed = false;
            while(!eof()) {
                if(current() == '\n') {
                    at_line_start_ = true;
                    result.at_line_start = true;
                }
                if(current() == '*' && peek_char() == '/') {
                    advance(2);
                    closed = true;
                    break;
                }
                advance();
            }
            if(!closed) {
                offset_ = start;
                break;
            }
            continue;
        }

        break;
    }

    if(!eof()) {
        at_line_start_ = false;
    }

    return result;
}

auto lexer::lex_token(token_flags flags) -> token
{
    auto const start = offset_;
    auto const ch = current();

    if(ch == '/' && peek_char() == '*') {
        return lex_unterminated_block_comment(start, flags);
    }

    if(detail::is_identifier_start(ch)) {
        return lex_identifier_or_keyword(start, flags);
    }

    if(std::isdigit(static_cast<unsigned char>(ch)) != 0) {
        return lex_number(start, flags);
    }

    if(ch == '"') {
        return lex_string_literal(start, flags);
    }

    if(ch == '\'') {
        return lex_char_literal(start, flags);
    }

    for(auto const& [spelling, kind] : detail::punctuation_table) {
        if(current_source().substr(offset_).starts_with(spelling)) {
            advance(spelling.size());
            return make_token(kind, start, offset_, flags);
        }
    }

    return lex_invalid_character(start, flags);
}

auto lexer::lex_identifier_or_keyword(std::size_t start, token_flags flags) -> token
{
    while(!eof() && detail::is_identifier_continue(current())) {
        advance();
    }

    auto const text = current_source().substr(start, offset_ - start);
    if(auto const keyword = detail::keyword_kind(text)) {
        return make_token(*keyword, start, offset_, flags);
    }

    return make_token(token_kind::identifier, start, offset_, flags);
}

auto lexer::lex_number(std::size_t start, token_flags flags) -> token
{
    while(!eof() && std::isdigit(static_cast<unsigned char>(current())) != 0) {
        advance();
    }

    auto is_float = false;
    if(!eof() && current() == '.' && std::isdigit(static_cast<unsigned char>(peek_char())) != 0) {
        is_float = true;
        advance();
        while(!eof() && std::isdigit(static_cast<unsigned char>(current())) != 0) {
            advance();
        }
    }

    if(!eof() && (current() == 'e' || current() == 'E')) {
        auto const exponent_start = offset_;
        auto probe = exponent_start + 1;
        if(probe < current_source().size()
           && (current_source()[probe] == '+' || current_source()[probe] == '-')) {
            ++probe;
        }
        if(probe < current_source().size()
           && std::isdigit(static_cast<unsigned char>(current_source()[probe])) != 0) {
            is_float = true;
            offset_ = probe + 1;
            while(!eof() && std::isdigit(static_cast<unsigned char>(current())) != 0) {
                advance();
            }
        }
    }

    if(!eof() && detail::is_identifier_start(current())) {
        return lex_invalid_after_number(start, flags);
    }

    return make_token(is_float ? token_kind::float_literal : token_kind::integer_literal, start, offset_, flags);
}

auto lexer::lex_string_literal(std::size_t start, token_flags flags) -> token
{
    advance();
    auto has_invalid_escape = false;

    while(!eof()) {
        if(current() == '"') {
            advance();
            if(has_invalid_escape) {
                return make_token(token_kind::invalid, start, offset_, flags | token_flags::recovered);
            }
            return make_token(token_kind::string_literal, start, offset_, flags);
        }
        if(current() == '\n') {
            report(diagnostic_code::unterminated_string_literal, "unterminated string literal", start, offset_);
            return make_token(token_kind::invalid, start, offset_,
                              flags | token_flags::unterminated | token_flags::recovered);
        }
        if(current() == '\\') {
            auto const escape = consume_escape_sequence(start, diagnostic_code::unterminated_string_literal);
            if(escape == escape_result::unterminated) {
                return make_token(token_kind::invalid, start, offset_,
                                  flags | token_flags::unterminated | token_flags::recovered);
            }
            if(escape == escape_result::invalid) {
                has_invalid_escape = true;
            }
            continue;
        }
        advance();
    }

    report(diagnostic_code::unterminated_string_literal, "unterminated string literal", start, offset_);
    return make_token(token_kind::invalid, start, offset_, flags | token_flags::unterminated | token_flags::recovered);
}

auto lexer::lex_char_literal(std::size_t start, token_flags flags) -> token
{
    advance();
    auto content_count = 0U;
    auto has_invalid_escape = false;

    while(!eof()) {
        if(current() == '\'') {
            advance();
            if(has_invalid_escape) {
                return make_token(token_kind::invalid, start, offset_, flags | token_flags::recovered);
            }
            if(content_count == 1) {
                return make_token(token_kind::char_literal, start, offset_, flags);
            }
            report(diagnostic_code::invalid_char_literal, "invalid char literal", start, offset_);
            return make_token(token_kind::invalid, start, offset_, flags | token_flags::recovered);
        }
        if(current() == '\n') {
            report(diagnostic_code::unterminated_char_literal, "unterminated char literal", start, offset_);
            return make_token(token_kind::invalid, start, offset_,
                              flags | token_flags::unterminated | token_flags::recovered);
        }
        if(current() == '\\') {
            auto const escape = consume_escape_sequence(start, diagnostic_code::unterminated_char_literal);
            if(escape == escape_result::unterminated) {
                return make_token(token_kind::invalid, start, offset_,
                                  flags | token_flags::unterminated | token_flags::recovered);
            }
            if(escape == escape_result::invalid) {
                has_invalid_escape = true;
            }
            ++content_count;
            continue;
        }
        ++content_count;
        advance();
    }

    report(diagnostic_code::unterminated_char_literal, "unterminated char literal", start, offset_);
    return make_token(token_kind::invalid, start, offset_, flags | token_flags::unterminated | token_flags::recovered);
}

auto lexer::consume_escape_sequence(std::size_t literal_start, diagnostic_code unterminated_code)
    -> escape_result
{
    auto const escape_start = offset_;
    advance();

    if(eof()) {
        report(unterminated_code,
               unterminated_code == diagnostic_code::unterminated_string_literal
                   ? "unterminated string literal"
                   : "unterminated char literal",
               literal_start,
               offset_);
        return escape_result::unterminated;
    }

    if(current() == '\n') {
        report(unterminated_code,
               unterminated_code == diagnostic_code::unterminated_string_literal
                   ? "unterminated string literal"
                   : "unterminated char literal",
               literal_start,
               offset_);
        return escape_result::unterminated;
    }

    if(detail::is_simple_escape(current())) {
        advance();
        return escape_result::valid;
    }

    if(detail::is_octal_digit(current())) {
        advance();
        std::ranges::for_each (
            std::views::iota(0uz, 2uz)
            | std::views::take_while([&](std::size_t) {
                return !eof() && detail::is_octal_digit(current());
            }),
            [&](auto) {
                advance();
            }
        );
        return escape_result::valid;
    }

    if(current() == 'x') {
        advance();
        if(eof() || !detail::is_hex_digit(current())) {
            report(diagnostic_code::invalid_escape_sequence, "invalid escape sequence", escape_start, offset_);
            return escape_result::invalid;
        }
        while(!eof() && detail::is_hex_digit(current())) {
            advance();
        }
        return escape_result::valid;
    }

    advance();
    report(diagnostic_code::invalid_escape_sequence, "invalid escape sequence", escape_start, offset_);
    return escape_result::invalid;
}

auto lexer::lex_invalid_after_number(std::size_t start, token_flags flags) -> token
{
    while(!eof() && !detail::is_recovery_delimiter(current())) {
        advance();
    }

    report(diagnostic_code::invalid_number_suffix, "invalid numeric suffix", start, offset_);
    return make_token(token_kind::invalid, start, offset_, flags | token_flags::recovered);
}

auto lexer::lex_invalid_character(std::size_t start, token_flags flags) -> token
{
    advance();
    report(diagnostic_code::invalid_character, "invalid character", start, offset_);
    return make_token(token_kind::invalid, start, offset_, flags | token_flags::recovered);
}

auto lexer::lex_unterminated_block_comment(std::size_t start, token_flags flags) -> token
{
    advance(2);
    while(!eof()) {
        if(current() == '\n') {
            at_line_start_ = true;
        }
        advance();
    }

    report(diagnostic_code::unterminated_block_comment, "unterminated block comment", start, offset_);
    return make_token(token_kind::invalid, start, offset_, flags | token_flags::unterminated | token_flags::recovered);
}
