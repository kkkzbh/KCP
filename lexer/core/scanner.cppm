export module lexer.scanner;

import std;
import lexer.source;
import lexer.token;
import lexer.diagnostic;

export namespace front {

class lexer {
public:
    lexer(source_manager const& sources, file_id file, diagnostic_sink& sink);

    auto reset(file_id file) -> void;
    [[nodiscard]] auto peek() -> token;
    auto next() -> token;
    [[nodiscard]] auto tokenize_all() -> std::vector<token>;

private:
    struct trivia_state {
        bool saw_space{};
        bool at_line_start{true};
    };

    source_manager const* sources_{};
    diagnostic_sink* sink_{};
    file_id file_{};
    std::size_t offset_{};
    bool has_peeked_{};
    token peeked_{};
    bool at_line_start_{true};

    [[nodiscard]] auto current_source() const -> std::string_view;
    [[nodiscard]] auto eof() const -> bool;
    [[nodiscard]] auto current() const -> char;
    [[nodiscard]] auto peek_char(std::size_t lookahead = 1) const -> char;
    auto advance(std::size_t count = 1) -> void;
    [[nodiscard]] auto make_token(token_kind kind, std::size_t start, std::size_t end,
        token_flags flags) const -> token;
    auto report(diagnostic_code code, std::string message, std::size_t start, std::size_t end)
        -> void;
    [[nodiscard]] auto skip_trivia() -> trivia_state;
    [[nodiscard]] auto lex_token(token_flags flags) -> token;
    [[nodiscard]] auto lex_identifier_or_keyword(std::size_t start, token_flags flags) -> token;
    [[nodiscard]] auto lex_number(std::size_t start, token_flags flags) -> token;
    [[nodiscard]] auto lex_string_literal(std::size_t start, token_flags flags) -> token;
    [[nodiscard]] auto lex_char_literal(std::size_t start, token_flags flags) -> token;
    [[nodiscard]] auto lex_invalid_after_number(std::size_t start, token_flags flags) -> token;
    [[nodiscard]] auto lex_invalid_character(std::size_t start, token_flags flags) -> token;
    [[nodiscard]] auto lex_unterminated_block_comment(std::size_t start, token_flags flags) -> token;
};

} // namespace front

namespace front {

namespace detail {

constexpr auto punctuation_table = std::to_array({
    std::pair{std::string_view{"::"}, token_kind::colon_colon},
    std::pair{std::string_view{"->"}, token_kind::arrow},
    std::pair{std::string_view{"=="}, token_kind::equal_equal},
    std::pair{std::string_view{"!="}, token_kind::bang_equal},
    std::pair{std::string_view{"<="}, token_kind::less_equal},
    std::pair{std::string_view{">="}, token_kind::greater_equal},
    std::pair{std::string_view{"<<"}, token_kind::less_less},
    std::pair{std::string_view{">>"}, token_kind::greater_greater},
    std::pair{std::string_view{"++"}, token_kind::plus_plus},
    std::pair{std::string_view{"--"}, token_kind::minus_minus},
    std::pair{std::string_view{"("}, token_kind::l_paren},
    std::pair{std::string_view{")"}, token_kind::r_paren},
    std::pair{std::string_view{"{"}, token_kind::l_brace},
    std::pair{std::string_view{"}"}, token_kind::r_brace},
    std::pair{std::string_view{"["}, token_kind::l_bracket},
    std::pair{std::string_view{"]"}, token_kind::r_bracket},
    std::pair{std::string_view{","}, token_kind::comma},
    std::pair{std::string_view{";"}, token_kind::semicolon},
    std::pair{std::string_view{":"}, token_kind::colon},
    std::pair{std::string_view{"."}, token_kind::dot},
    std::pair{std::string_view{"+"}, token_kind::plus},
    std::pair{std::string_view{"-"}, token_kind::minus},
    std::pair{std::string_view{"*"}, token_kind::star},
    std::pair{std::string_view{"/"}, token_kind::slash},
    std::pair{std::string_view{"%"}, token_kind::percent},
    std::pair{std::string_view{"="}, token_kind::equal},
    std::pair{std::string_view{"<"}, token_kind::less},
    std::pair{std::string_view{">"}, token_kind::greater},
    std::pair{std::string_view{"&"}, token_kind::amp},
    std::pair{std::string_view{"|"}, token_kind::pipe},
    std::pair{std::string_view{"^"}, token_kind::caret},
    std::pair{std::string_view{"~"}, token_kind::tilde},
    std::pair{std::string_view{"?"}, token_kind::question},
});

constexpr auto keyword_table = std::to_array({
    std::pair{std::string_view{"let"}, token_kind::kw_let},
    std::pair{std::string_view{"const"}, token_kind::kw_const},
    std::pair{std::string_view{"if"}, token_kind::kw_if},
    std::pair{std::string_view{"else"}, token_kind::kw_else},
    std::pair{std::string_view{"while"}, token_kind::kw_while},
    std::pair{std::string_view{"do"}, token_kind::kw_do},
    std::pair{std::string_view{"for"}, token_kind::kw_for},
    std::pair{std::string_view{"break"}, token_kind::kw_break},
    std::pair{std::string_view{"continue"}, token_kind::kw_continue},
    std::pair{std::string_view{"return"}, token_kind::kw_return},
    std::pair{std::string_view{"import"}, token_kind::kw_import},
    std::pair{std::string_view{"export"}, token_kind::kw_export},
    std::pair{std::string_view{"module"}, token_kind::kw_module},
    std::pair{std::string_view{"struct"}, token_kind::kw_struct},
    std::pair{std::string_view{"impl"}, token_kind::kw_impl},
    std::pair{std::string_view{"trait"}, token_kind::kw_trait},
    std::pair{std::string_view{"true"}, token_kind::kw_true},
    std::pair{std::string_view{"false"}, token_kind::kw_false},
    std::pair{std::string_view{"and"}, token_kind::kw_and},
    std::pair{std::string_view{"or"}, token_kind::kw_or},
    std::pair{std::string_view{"not"}, token_kind::kw_not},
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
    constexpr auto delimiters = std::string_view{" \t\r\n(){}[],:;.+-*/%<>=&|^~?\"'"};
    return delimiters.contains(ch);
}

[[nodiscard]] auto keyword_kind(std::string_view text) -> std::optional<token_kind>
{
    for (auto const& [keyword, kind] : keyword_table) {
        if (keyword == text) {
            return kind;
        }
    }

    return std::nullopt;
}

} // namespace detail

lexer::lexer(source_manager const& sources, file_id file, diagnostic_sink& sink)
    : sources_(&sources), sink_(&sink), file_(file)
{
}

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
    if (!has_peeked_) {
        peeked_ = next();
        has_peeked_ = true;
    }

    return peeked_;
}

auto lexer::next() -> token
{
    if (has_peeked_) {
        has_peeked_ = false;
        return peeked_;
    }

    auto const trivia = skip_trivia();
    auto flags = token_flags::none;
    if (trivia.saw_space) {
        flags |= token_flags::leading_space;
    }
    if (trivia.at_line_start) {
        flags |= token_flags::start_of_line;
    }

    if (eof()) {
        return make_token(token_kind::eof, offset_, offset_, flags);
    }

    return lex_token(flags);
}

auto lexer::tokenize_all() -> std::vector<token>
{
    auto result = std::vector<token>{};
    for (;;) {
        auto const current_token = next();
        result.push_back(current_token);
        if (current_token.kind == token_kind::eof) {
            break;
        }
    }

    return result;
}

auto lexer::current_source() const -> std::string_view
{
    return sources_->text(file_);
}

auto lexer::eof() const -> bool
{
    return offset_ >= current_source().size();
}

auto lexer::current() const -> char
{
    return eof() ? '\0' : current_source()[offset_];
}

auto lexer::peek_char(std::size_t lookahead) const -> char
{
    auto const index = offset_ + lookahead;
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
        .source_span = span{.file = file_, .start = start, .end = end},
        .flags = flags,
    };
}

auto lexer::report(diagnostic_code code, std::string message, std::size_t start, std::size_t end)
    -> void
{
    sink_->report(diagnostic{
        .severity = diagnostic_severity::error,
        .code = code,
        .message = std::move(message),
        .primary_span = span{.file = file_, .start = start, .end = end},
    });
}

auto lexer::skip_trivia() -> trivia_state
{
    auto result = trivia_state{.saw_space = false, .at_line_start = at_line_start_};

    while (!eof()) {
        auto const ch = current();

        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            result.saw_space = true;
            if (ch == '\n') {
                at_line_start_ = true;
                result.at_line_start = true;
            }
            advance();
            continue;
        }

        if (ch == '/' && peek_char() == '/') {
            result.saw_space = true;
            advance(2);
            while (!eof() && current() != '\n') {
                advance();
            }
            continue;
        }

        if (ch == '/' && peek_char() == '*') {
            result.saw_space = true;
            auto const start = offset_;
            advance(2);
            auto closed = false;
            while (!eof()) {
                if (current() == '\n') {
                    at_line_start_ = true;
                    result.at_line_start = true;
                }
                if (current() == '*' && peek_char() == '/') {
                    advance(2);
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                offset_ = start;
                break;
            }
            continue;
        }

        break;
    }

    if (!eof()) {
        at_line_start_ = false;
    }

    return result;
}

auto lexer::lex_token(token_flags flags) -> token
{
    auto const start = offset_;
    auto const ch = current();

    if (ch == '/' && peek_char() == '*') {
        return lex_unterminated_block_comment(start, flags);
    }

    if (detail::is_identifier_start(ch)) {
        return lex_identifier_or_keyword(start, flags);
    }

    if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
        return lex_number(start, flags);
    }

    if (ch == '"') {
        return lex_string_literal(start, flags);
    }

    if (ch == '\'') {
        return lex_char_literal(start, flags);
    }

    for (auto const& [spelling, kind] : detail::punctuation_table) {
        if (current_source().substr(offset_).starts_with(spelling)) {
            advance(spelling.size());
            return make_token(kind, start, offset_, flags);
        }
    }

    return lex_invalid_character(start, flags);
}

auto lexer::lex_identifier_or_keyword(std::size_t start, token_flags flags) -> token
{
    while (!eof() && detail::is_identifier_continue(current())) {
        advance();
    }

    auto const text = current_source().substr(start, offset_ - start);
    if (auto const keyword = detail::keyword_kind(text)) {
        return make_token(*keyword, start, offset_, flags);
    }

    return make_token(token_kind::identifier, start, offset_, flags);
}

auto lexer::lex_number(std::size_t start, token_flags flags) -> token
{
    while (!eof() && std::isdigit(static_cast<unsigned char>(current())) != 0) {
        advance();
    }

    auto is_float = false;
    if (!eof() && current() == '.' && std::isdigit(static_cast<unsigned char>(peek_char())) != 0) {
        is_float = true;
        advance();
        while (!eof() && std::isdigit(static_cast<unsigned char>(current())) != 0) {
            advance();
        }
    }

    if (!eof() && (current() == 'e' || current() == 'E')) {
        auto const exponent_start = offset_;
        auto probe = exponent_start + 1;
        if (probe < current_source().size()
            && (current_source()[probe] == '+' || current_source()[probe] == '-')) {
            ++probe;
        }
        if (probe < current_source().size()
            && std::isdigit(static_cast<unsigned char>(current_source()[probe])) != 0) {
            is_float = true;
            offset_ = probe + 1;
            while (!eof() && std::isdigit(static_cast<unsigned char>(current())) != 0) {
                advance();
            }
        }
    }

    if (!eof() && detail::is_identifier_start(current())) {
        return lex_invalid_after_number(start, flags);
    }

    return make_token(is_float ? token_kind::float_literal : token_kind::integer_literal, start, offset_, flags);
}

auto lexer::lex_string_literal(std::size_t start, token_flags flags) -> token
{
    advance();
    while (!eof()) {
        if (current() == '\\') {
            advance();
            if (!eof()) {
                advance();
            }
            continue;
        }
        if (current() == '"') {
            advance();
            return make_token(token_kind::string_literal, start, offset_, flags);
        }
        if (current() == '\n') {
            report(diagnostic_code::unterminated_string_literal, "unterminated string literal", start, offset_);
            return make_token(token_kind::invalid, start, offset_, flags | token_flags::unterminated | token_flags::recovered);
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

    while (!eof()) {
        if (current() == '\n') {
            report(diagnostic_code::unterminated_char_literal, "unterminated char literal", start, offset_);
            return make_token(token_kind::invalid, start, offset_, flags | token_flags::unterminated | token_flags::recovered);
        }
        if (current() == '\\') {
            ++content_count;
            advance();
            if (eof()) {
                break;
            }
            advance();
            continue;
        }
        if (current() == '\'') {
            advance();
            if (content_count == 1) {
                return make_token(token_kind::char_literal, start, offset_, flags);
            }
            report(diagnostic_code::invalid_char_literal, "invalid char literal", start, offset_);
            return make_token(token_kind::invalid, start, offset_, flags | token_flags::recovered);
        }
        ++content_count;
        advance();
    }

    report(diagnostic_code::unterminated_char_literal, "unterminated char literal", start, offset_);
    return make_token(token_kind::invalid, start, offset_, flags | token_flags::unterminated | token_flags::recovered);
}

auto lexer::lex_invalid_after_number(std::size_t start, token_flags flags) -> token
{
    while (!eof() && !detail::is_recovery_delimiter(current())) {
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
    while (!eof()) {
        if (current() == '\n') {
            at_line_start_ = true;
        }
        advance();
    }

    report(diagnostic_code::unterminated_block_comment, "unterminated block comment", start, offset_);
    return make_token(token_kind::invalid, start, offset_, flags | token_flags::unterminated | token_flags::recovered);
}

} // namespace front
