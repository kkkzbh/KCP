/// @brief 词法分析器驱动模块。
/// @details 本模块负责状态机驱动（peek/next/reset/tokenize_all）、trivia 处理、
///          预处理错误对接与 token 分派，并在 `lexer<Sink>` 内部完成字面量边界切分。
///          字符分类与标点/关键字表外置于 `lexer.charset` / `lexer.tables`。
export module lexer.scanner;

import std;
import lexer.source;
import lexer.token;
import lexer.diagnostic;
import lexer.charset;
import lexer.tables;
import preprocessor;

/// @brief 词法分析器模板，负责把预处理后的源文本切分为 token 序列。
/// @tparam Sink 诊断接收器类型，需要满足 `diagnostic_sink` 概念。
/// @details 扫描器先消费预处理阶段输出的规范化文本，再按最长匹配与恢复规则产出 token。
export template<diagnostic_sink Sink>
struct lexer
{
    /// @brief 构造词法分析器，并立即装载指定文件的预处理结果。
    lexer(source_manager const& sources, file_id file, Sink& sink) : sources_(sources), sink_(sink)
    {
        reset(file);
    }

    /// @brief 重新绑定到另一个文件，并清空当前扫描状态。
    auto reset(file_id file) -> void
    {
        file_ = file;
        offset_ = 0;
        preprocessed_ = preprocess(sources_, file_);
        has_peeked_ = false;
        peeked_ = {};
        at_line_start_ = true;
        next_issue_index_ = 0;
    }

    /// @brief 返回当前 token，但不推进扫描位置。
    auto peek() -> token
    {
        if(not has_peeked_) {
            peeked_ = next();
            has_peeked_ = true;
        }
        return peeked_;
    }

    /// @brief 返回下一个 token，并推进扫描位置。
    auto next() -> token
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

    /// @brief 反复扫描直到文件结束，并返回全部 token（含末尾 `eof`）。
    auto tokenize_all() -> std::vector<token>
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

private:
    /// @brief 跳过 trivia 过程中收集到的状态。
    struct trivia_state
    {
        bool saw_space{};
        bool at_line_start{ true };
    };

    auto current_source() const -> std::string_view { return preprocessed_.normalized_text; }

    auto eof() const -> bool { return offset_ >= current_source().size(); }

    auto current() const -> char { return eof() ? '\0' : current_source()[offset_]; }

    auto make_token(token_kind kind, std::size_t start, std::size_t end, token_flags flags) const -> token
    {
        return token{
            .kind = kind,
            .source_span = span{ .file = file_, .start = start, .end = end },
            .flags = flags,
        };
    }

    auto report(diagnostic_code code, std::string_view message, std::size_t start, std::size_t end) const -> void
    {
        sink_.report(diagnostic{
            .severity = diagnostic_severity::error,
            .code = code,
            .message = std::string{ message },
            .primary_span = span{ .file = file_, .start = start, .end = end },
        });
    }

    auto skip_trivia() -> trivia_state
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
                ++offset_;
                continue;
            }

            break;
        }

        if(not eof() and preprocess_issue_at_offset() == nullptr) {
            at_line_start_ = false;
        }

        return result;
    }

    auto preprocess_issue_at_offset() -> preprocess_issue const*
    {
        while(next_issue_index_ < preprocessed_.issues.size()
              and preprocessed_.issues[next_issue_index_].source_span.end <= offset_) {
            ++next_issue_index_;
        }
        if(next_issue_index_ >= preprocessed_.issues.size()) {
            return nullptr;
        }
        auto const& issue = preprocessed_.issues[next_issue_index_];
        return issue.source_span.start == offset_ ? &issue : nullptr;
    }

    auto issue_contains_newline(preprocess_issue const& issue) const -> bool
    {
        auto const source = current_source().substr (
            issue.source_span.start,
            issue.source_span.end - issue.source_span.start
        );
        return source.contains('\n');
    }

    auto lex_preprocess_issue(token_flags flags, preprocess_issue const& issue) -> token
    {
        switch(issue.kind) {
            case preprocess_issue_kind::unterminated_block_comment:
                report (
                    diagnostic_code::unterminated_block_comment,
                    "unterminated block comment",
                    issue.source_span.start,
                    issue.source_span.end
                );
                break;
        }

        offset_ = issue.source_span.end;
        at_line_start_ = issue_contains_newline(issue);
        ++next_issue_index_;

        return token{
            .kind = token_kind::invalid,
            .source_span = issue.source_span,
            .flags = flags | token_flags::unterminated | token_flags::recovered,
        };
    }

    auto lex_token(token_flags flags) -> token
    {
        auto start = offset_;
        auto ch = current();

        if(is_identifier_start(ch)) {
            return lex_identifier_or_keyword(start, flags);
        }

        if(is_decimal_digit(ch)) {
            return lex_number_literal(start, flags);
        }

        if(ch == '"') {
            return lex_string_literal(start, flags);
        }

        if(ch == '\'') {
            return lex_char_literal(start, flags);
        }

        if(auto const hit = match_punctuation(current_source().substr(offset_))) {
            auto const [kind, length] = *hit;
            offset_ += length;
            return make_token(kind, start, offset_, flags);
        }

        ++offset_;
        report(diagnostic_code::invalid_character, "invalid character", start, offset_);
        return make_token(token_kind::invalid, start, offset_, flags | token_flags::recovered);
    }

    auto lex_identifier_or_keyword(std::size_t start, token_flags flags) -> token
    {
        while(not eof() and is_identifier_continue(current())) {
            ++offset_;
        }
        auto const text = current_source().substr(start, offset_ - start);
        if(auto const keyword = keyword_kind(text)) {
            return make_token(*keyword, start, offset_, flags);
        }
        return make_token(token_kind::identifier, start, offset_, flags);
    }

    /// @brief 反斜杠转义序列在字面量内部的解析结果。
    enum class escape_result
    {
        valid,        ///< 转义序列合法。
        invalid,      ///< 转义序列非法，但字面量仍可继续恢复。
        unterminated, ///< 转义序列没有正常结束（EOF 或换行）。
    };

    /// @brief 在字符串/字符字面量内部消费一段反斜杠转义序列。
    /// @details 进入时 `offset_` 须指向 `\\`；返回时已推进到转义末尾。诊断直接经
    ///          `report()` 发往 sink。规则与 C 经典转义集合保持一致：简单转义、八进制
    ///          1~3 位、`\xH+`，其余形如 `\z` 视作非法但可恢复。
    auto consume_escape_sequence (
        std::size_t literal_start,
        diagnostic_code unterminated_code,
        std::string_view unterminated_message
    ) -> escape_result
    {
        using namespace std::literals;

        auto const src = current_source();
        auto const escape_start = offset_;
        ++offset_; // 吞掉反斜杠

        if(offset_ >= src.size()) {
            report(unterminated_code, unterminated_message, literal_start, offset_);
            return escape_result::unterminated;
        }

        if(src[offset_] == '\n') {
            report(unterminated_code, unterminated_message, literal_start, offset_);
            return escape_result::unterminated;
        }

        if(is_simple_escape(src[offset_])) {
            ++offset_;
            return escape_result::valid;
        }

        if(is_octal_digit(src[offset_])) {
            ++offset_;
            for(auto extra = 0; extra < 2 and offset_ < src.size() and is_octal_digit(src[offset_]); ++extra) {
                ++offset_;
            }
            return escape_result::valid;
        }

        if(src[offset_] == 'x') {
            ++offset_;
            if(offset_ >= src.size() or not is_hex_digit(src[offset_])) {
                report(diagnostic_code::invalid_escape_sequence, "invalid escape sequence"sv, escape_start, offset_);
                return escape_result::invalid;
            }
            while(offset_ < src.size() and is_hex_digit(src[offset_])) {
                ++offset_;
            }
            return escape_result::valid;
        }

        ++offset_;
        report(diagnostic_code::invalid_escape_sequence, "invalid escape sequence"sv, escape_start, offset_);
        return escape_result::invalid;
    }

    /// @brief 扫描数字字面量（整数或浮点）。`offset_` 入口即字面量起始（首位十进制数字）。
    auto lex_number_literal(std::size_t start, token_flags flags) -> token
    {
        using namespace std::literals;

        auto const src = current_source();
        auto invalid_number = [&](std::size_t end) -> token {
            offset_ = end;
            report(diagnostic_code::invalid_number_suffix, "invalid numeric suffix"sv, start, offset_);
            return make_token(token_kind::invalid, start, offset_, flags | token_flags::recovered);
        };

        offset_ = start;
        while(offset_ < src.size() and is_decimal_digit(src[offset_])) {
            ++offset_;
        }
        auto const digit_end = offset_;

        auto is_float = (
            offset_ < src.size() and src[offset_] == '.'
            and (offset_ + 1 >= src.size() or not is_identifier_start(src[offset_ + 1]))
        );
        if(is_float) {
            ++offset_;
            while(offset_ < src.size() and is_decimal_digit(src[offset_])) {
                ++offset_;
            }
        }

        if(offset_ < src.size() and is_exponent_marker(src[offset_])) {
            auto probe = offset_ + 1;
            if(probe < src.size() and (src[probe] == '+' or src[probe] == '-')) {
                ++probe;
            }
            if(probe >= src.size() or not is_decimal_digit(src[probe])) {
                return invalid_number(probe);
            }

            is_float = true;
            offset_ = probe + 1;
            while(offset_ < src.size() and is_decimal_digit(src[offset_])) {
                ++offset_;
            }
        }

        if(offset_ < src.size() and is_identifier_start(src[offset_])) {
            while(offset_ < src.size() and not is_recovery_delimiter(src[offset_])) {
                ++offset_;
            }
            return invalid_number(offset_);
        }

        if(not is_float and src[start] == '0' and digit_end > start + 1) {
            return invalid_number(digit_end);
        }

        auto const kind = is_float ? token_kind::float_literal : token_kind::integer_literal;
        return make_token(kind, start, offset_, flags);
    }

    /// @brief 扫描字符串字面量。`offset_` 入口指向起始 `"`。
    auto lex_string_literal(std::size_t start, token_flags flags) -> token
    {
        using namespace std::literals;

        auto constexpr unterminated_message = "unterminated string literal"sv;
        auto const src = current_source();
        offset_ = start + 1; // 跳过开头的 "
        auto has_invalid_escape = false;

        while(offset_ < src.size()) {
            auto const ch = src[offset_];
            if(ch == '"') {
                ++offset_;
                if(has_invalid_escape) {
                    return make_token(token_kind::invalid, start, offset_, flags | token_flags::recovered);
                }
                return make_token(token_kind::string_literal, start, offset_, flags);
            }
            if(ch == '\n') {
                report(diagnostic_code::unterminated_string_literal, unterminated_message, start, offset_);
                return make_token(token_kind::invalid, start, offset_,
                    flags | token_flags::unterminated | token_flags::recovered);
            }
            if(ch == '\\') {
                auto const result = consume_escape_sequence(start,
                    diagnostic_code::unterminated_string_literal, unterminated_message);
                if(result == escape_result::unterminated) {
                    return make_token(token_kind::invalid, start, offset_,
                        flags | token_flags::unterminated | token_flags::recovered);
                }
                if(result == escape_result::invalid) {
                    has_invalid_escape = true;
                }
                continue;
            }
            ++offset_;
        }

        report(diagnostic_code::unterminated_string_literal, unterminated_message, start, offset_);
        return make_token(token_kind::invalid, start, offset_,
            flags | token_flags::unterminated | token_flags::recovered);
    }

    /// @brief 扫描字符字面量。`offset_` 入口指向起始 `'`。
    auto lex_char_literal(std::size_t start, token_flags flags) -> token
    {
        using namespace std::literals;

        auto constexpr unterminated_message = "unterminated char literal"sv;
        auto const src = current_source();
        offset_ = start + 1; // 跳过开头的 '
        auto content_count = 0U;
        auto has_invalid_escape = false;

        while(offset_ < src.size()) {
            auto const ch = src[offset_];
            if(ch == '\'') {
                ++offset_;
                if(has_invalid_escape) {
                    return make_token(token_kind::invalid, start, offset_, flags | token_flags::recovered);
                }
                if(content_count == 1) {
                    return make_token(token_kind::char_literal, start, offset_, flags);
                }
                report(diagnostic_code::invalid_char_literal, "invalid char literal"sv, start, offset_);
                return make_token(token_kind::invalid, start, offset_, flags | token_flags::recovered);
            }
            if(ch == '\n') {
                report(diagnostic_code::unterminated_char_literal, unterminated_message, start, offset_);
                return make_token(token_kind::invalid, start, offset_,
                    flags | token_flags::unterminated | token_flags::recovered);
            }
            if(ch == '\\') {
                auto const result = consume_escape_sequence(start,
                    diagnostic_code::unterminated_char_literal, unterminated_message);
                if(result == escape_result::unterminated) {
                    return make_token(token_kind::invalid, start, offset_,
                        flags | token_flags::unterminated | token_flags::recovered);
                }
                if(result == escape_result::invalid) {
                    has_invalid_escape = true;
                }
                ++content_count;
                continue;
            }
            ++content_count;
            ++offset_;
        }

        report(diagnostic_code::unterminated_char_literal, unterminated_message, start, offset_);
        return make_token(token_kind::invalid, start, offset_,
            flags | token_flags::unterminated | token_flags::recovered);
    }

    source_manager const& sources_;
    Sink& sink_;
    file_id file_{};
    std::size_t offset_{};
    preprocessed_file preprocessed_{};
    bool has_peeked_{};
    token peeked_{};
    bool at_line_start_{ true };
    std::size_t next_issue_index_{};
};

/// @brief 为 `lexer` 模板提供类模板实参推导。
export template<diagnostic_sink Sink> lexer(source_manager const& sources, file_id file, Sink& sink) -> lexer<Sink>;
