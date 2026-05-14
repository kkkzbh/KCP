export module lexer.scanner_engine;

import std;
import source;
import lexer.token;
import lexer.diagnostic;
import lexer.charset;
import lexer.diagnostic_reporter;
import lexer.keyword;
import lexer.punctuation;
import lexer.literal;
import preprocessor;

/// @brief 非模板词法扫描状态机。
export struct scanner_engine
{
    scanner_engine(source_manager const& sources, file_id file, diagnostic_reporter reporter);

    auto reset(file_id file) -> void;
    auto peek() -> token;
    auto next() -> token;
    auto tokenize_all() -> std::vector<token>;

private:
    struct trivia_state
    {
        bool saw_space{};
        bool at_line_start{ true };
    };

    auto current_source() const -> std::string_view;
    auto eof() const -> bool;
    auto peek_char(std::size_t distance = 0) const -> char;
    auto current() const -> char;
    auto advance(std::size_t count = 1) -> void;
    auto current_pos() const -> byte_pos;
    auto to_global(std::size_t local) const -> byte_pos;
    auto make_token(token_kind kind, std::size_t start, std::size_t end, token_flags flags) const -> token;
    auto report(lexer_diagnostic_code code, std::string_view message, std::size_t start, std::size_t end) const -> void;
    auto emit_diagnostics(std::vector<scanner_diagnostic_request> const& diagnostics) const -> void;
    auto skip_trivia() -> trivia_state;
    auto preprocess_issue_at_offset() -> preprocess_issue const*;
    auto issue_contains_newline(preprocess_issue const& issue) const -> bool;
    auto lex_preprocess_issue(token_flags flags, preprocess_issue const& issue) -> token;
    auto lex_token(token_flags flags) -> token;
    auto make_punctuation_token(token_kind kind, std::size_t start, std::size_t length, token_flags flags) -> token;
    auto lex_identifier_or_keyword(std::size_t start, token_flags flags) -> token;
    auto lex_literal(literal_scan_result result, std::size_t start) -> token;

    source_manager const& sources_;
    diagnostic_reporter reporter_;
    file_id file_{};
    byte_pos file_start_{};
    std::size_t offset_{};
    preprocessed_file preprocessed_{};
    bool has_peeked_{};
    token peeked_{};
    bool at_line_start_{ true };
    std::size_t next_issue_index_{};
};

scanner_engine::scanner_engine(source_manager const& sources, file_id file, diagnostic_reporter reporter)
    : sources_(sources), reporter_(reporter)
{
    reset(file);
}

auto scanner_engine::reset(file_id file) -> void
{
    file_ = file;
    file_start_ = sources_.file_start(file);
    offset_ = 0;
    preprocessed_ = preprocess(sources_, file_);
    has_peeked_ = false;
    peeked_ = {};
    at_line_start_ = true;
    next_issue_index_ = 0;
}

auto scanner_engine::peek() -> token
{
    if(not has_peeked_) {
        peeked_ = next();
        has_peeked_ = true;
    }
    return peeked_;
}

auto scanner_engine::next() -> token
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

    if(auto const* issue = preprocess_issue_at_offset()) {
        return lex_preprocess_issue(flags, *issue);
    }

    return lex_token(flags);
}

auto scanner_engine::tokenize_all() -> std::vector<token>
{
    auto result = std::vector<token>{};
    while(true) {
        auto const current_token = next();
        result.push_back(current_token);
        if(current_token.kind == token_kind::eof) {
            break;
        }
    }
    return result;
}

auto scanner_engine::current_source() const -> std::string_view
{
    return preprocessed_.normalized_text;
}

auto scanner_engine::eof() const -> bool
{
    return offset_ >= current_source().size();
}

auto scanner_engine::peek_char(std::size_t distance) const -> char
{
    auto const index = offset_ + distance;
    auto const source = current_source();
    return index < source.size() ? source[index] : '\0';
}

auto scanner_engine::current() const -> char
{
    return peek_char();
}

auto scanner_engine::advance(std::size_t count) -> void
{
    offset_ += count;
}

auto scanner_engine::current_pos() const -> byte_pos
{
    return file_start_ + static_cast<byte_pos>(offset_);
}

auto scanner_engine::to_global(std::size_t local) const -> byte_pos
{
    return file_start_ + static_cast<byte_pos>(local);
}

auto scanner_engine::make_token(token_kind kind, std::size_t start, std::size_t end, token_flags flags) const -> token
{
    return token {
        .kind = kind,
        .span = source_span{ .start = to_global(start), .end = to_global(end) },
        .flags = flags,
    };
}

auto scanner_engine::report(lexer_diagnostic_code code, std::string_view message, std::size_t start, std::size_t end) const
    -> void
{
    reporter_.report(lexer_diagnostic {
        .severity = lexer_diagnostic_severity::error,
        .code = code,
        .message = std::string{ message },
        .primary_span = source_span{ .start = to_global(start), .end = to_global(end) },
    });
}

auto scanner_engine::emit_diagnostics(std::vector<scanner_diagnostic_request> const& diagnostics) const -> void
{
    for(auto const& value : diagnostics) {
        report(value.code, value.message, value.start, value.end);
    }
}

auto scanner_engine::skip_trivia() -> trivia_state
{
    auto result = trivia_state{ .saw_space = false, .at_line_start = at_line_start_ };

    while(not eof()) {
        if(auto const* issue = preprocess_issue_at_offset()) {
            result.saw_space = true;
            if(issue_contains_newline(*issue)) {
                result.at_line_start = true;
            }
            break;
        }

        auto const ch = current();
        if(is_space(ch)) {
            result.saw_space = true;
            if(ch == '\n') {
                at_line_start_ = true;
                result.at_line_start = true;
            }
            advance();
            continue;
        }

        break;
    }

    if(not eof() and preprocess_issue_at_offset() == nullptr) {
        at_line_start_ = false;
    }

    return result;
}

auto scanner_engine::preprocess_issue_at_offset() -> preprocess_issue const*
{
    auto const cursor = current_pos();
    while(next_issue_index_ < preprocessed_.issues.size()
          and preprocessed_.issues[next_issue_index_].span.end <= cursor) {
        ++next_issue_index_;
    }
    if(next_issue_index_ >= preprocessed_.issues.size()) {
        return nullptr;
    }
    auto const& issue = preprocessed_.issues[next_issue_index_];
    return issue.span.start == cursor ? &issue : nullptr;
}

auto scanner_engine::issue_contains_newline(preprocess_issue const& issue) const -> bool
{
    auto const local_start = static_cast<std::size_t>(issue.span.start - file_start_);
    auto const length = static_cast<std::size_t>(issue.span.end - issue.span.start);
    auto const source = current_source().substr(local_start, length);
    return source.contains('\n');
}

auto scanner_engine::lex_preprocess_issue(token_flags flags, preprocess_issue const& issue) -> token
{
    auto const local_start = static_cast<std::size_t>(issue.span.start - file_start_);
    auto const local_end = static_cast<std::size_t>(issue.span.end - file_start_);

    switch(issue.kind) {
        case preprocess_issue_kind::unterminated_block_comment:
            report (
                lexer_diagnostic_code::unterminated_block_comment,
                "unterminated block comment",
                local_start,
                local_end
            );
            break;
    }

    offset_ = local_end;
    at_line_start_ = issue_contains_newline(issue);
    ++next_issue_index_;

    return token{
        .kind = token_kind::invalid,
        .span = issue.span,
        .flags = flags | token_flags::unterminated | token_flags::recovered,
    };
}

auto scanner_engine::lex_token(token_flags flags) -> token
{
    auto const start = offset_;
    auto const ch = current();

    if(is_identifier_start(ch)) {
        return lex_identifier_or_keyword(start, flags);
    }

    if(is_decimal_digit(ch)) {
        return lex_literal(scan_number_literal(current_source(), start, flags), start);
    }

    if(ch == '"') {
        return lex_literal(scan_string_literal(current_source(), start, flags), start);
    }

    if(ch == '\'') {
        return lex_literal(scan_char_literal(current_source(), start, flags), start);
    }

    if(auto const punctuation = match_punctuation(current_source(), start)) {
        return make_punctuation_token(punctuation->kind, start, punctuation->length, flags);
    }

    advance();
    report(lexer_diagnostic_code::invalid_character, "invalid character", start, offset_);
    return make_token(token_kind::invalid, start, offset_, flags | token_flags::recovered);
}

auto scanner_engine::make_punctuation_token(
    token_kind kind,
    std::size_t start,
    std::size_t length,
    token_flags flags) -> token
{
    advance(length);
    return make_token(kind, start, offset_, flags);
}

auto scanner_engine::lex_identifier_or_keyword(std::size_t start, token_flags flags) -> token
{
    while(not eof() and is_identifier_continue(current())) {
        advance();
    }
    auto const text = current_source().substr(start, offset_ - start);
    if(auto const keyword = keyword_kind(text)) {
        return make_token(*keyword, start, offset_, flags);
    }
    return make_token(token_kind::identifier, start, offset_, flags);
}

auto scanner_engine::lex_literal(literal_scan_result result, std::size_t start) -> token
{
    offset_ = result.end;
    emit_diagnostics(result.diagnostics);
    return make_token(result.kind, start, offset_, result.flags);
}
