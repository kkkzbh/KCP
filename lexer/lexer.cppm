/// @brief 词法分析器公共 facade 与扫描状态机实现。
/// @details 对外提供一次性 `lex(preprocessed_file)` 入口；内部直接维护扫描状态和诊断列表。
export module lexer;

import std;
export import lexer.token;
export import diagnostic;
export import lexer.charset;
import preprocessor.preprocessed;
import lexer.cursor;
import lexer.keyword;
import lexer.punctuation;
import lexer.literal;

/// @brief 词法扫描状态机，负责把预处理后的源文本切分为 token 序列。
struct lexer
{
    /// @brief 构造词法分析器，并立即装载指定文件的预处理结果。
    lexer(preprocessed_file const& file)
    {
        reset(file);
    }

    /// @brief 重新绑定到另一个预处理文件，并清空当前扫描状态。
    auto reset(preprocessed_file const& file) -> void
    {
        cursor_.reset(file);
        diagnostics_.clear();
        has_peeked_ = false;
        peeked_ = {};
        at_line_start_ = true;
    }

    /// @brief 取出当前扫描产生的词法诊断。
    auto take_diagnostics() -> std::vector<diagnostic>
    {
        return diagnostics_.take();
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

        if(cursor_.eof()) {
            return cursor_.make_token(token_kind::eof, cursor_.offset(), cursor_.offset(), flags);
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
    struct trivia_state
    {
        bool saw_space{};
        bool at_line_start{ true };
    };

    auto skip_trivia() -> trivia_state
    {
        auto result = trivia_state{ .saw_space = false, .at_line_start = at_line_start_ };

        while(not cursor_.eof()) {
            auto const ch = cursor_.current();
            if(is_space(ch)) {
                result.saw_space = true;
                if(ch == '\n') {
                    at_line_start_ = true;
                    result.at_line_start = true;
                }
                cursor_.advance();
                continue;
            }

            break;
        }

        if(not cursor_.eof()) {
            at_line_start_ = false;
        }

        return result;
    }

    auto lex_token(token_flags flags) -> token
    {
        auto const start = cursor_.offset();
        auto const ch = cursor_.current();

        if(is_identifier_start(ch)) {
            return lex_identifier_or_keyword(start, flags);
        }

        if(is_decimal_digit(ch)) {
            return lex_literal(scan_number_literal(cursor_.source(), start, flags), start);
        }

        if(ch == '"') {
            return lex_literal(scan_string_literal(cursor_.source(), start, flags), start);
        }

        if(ch == '\'') {
            return lex_literal(scan_char_literal(cursor_.source(), start, flags), start);
        }

        if(auto const punctuation = match_punctuation(cursor_.source(), start)) {
            return make_punctuation_token(punctuation->kind, start, punctuation->length, flags);
        }

        cursor_.advance();
        diagnostics_.report(diagnostic_kind::invalid_character, cursor_.make_span(start, cursor_.offset()));
        return cursor_.make_token(token_kind::invalid, start, cursor_.offset(), flags | token_flags::recovered);
    }

    auto make_punctuation_token(
        token_kind kind,
        std::size_t start,
        std::size_t length,
        token_flags flags) -> token
    {
        cursor_.advance(length);
        return cursor_.make_token(kind, start, cursor_.offset(), flags);
    }

    auto lex_identifier_or_keyword(std::size_t start, token_flags flags) -> token
    {
        while(not cursor_.eof() and is_identifier_continue(cursor_.current())) {
            cursor_.advance();
        }
        auto const text = cursor_.source().substr(start, cursor_.offset() - start);
        if(auto const keyword = keyword_kind(text)) {
            return cursor_.make_token(*keyword, start, cursor_.offset(), flags);
        }
        auto token = cursor_.make_token(token_kind::identifier, start, cursor_.offset(), flags);
        token.text = std::string{ text };
        return token;
    }

    auto lex_literal(literal_scan_result result, std::size_t start) -> token
    {
        cursor_.set_offset(result.end);
        auto diagnostics = (
            result.diagnostics
            | std::views::transform([&](literal_diagnostic const& value) {
                return diagnostic {
                    .kind = value.kind,
                    .primary_span = cursor_.make_span(value.start, value.end),
                    .message = std::string{ value.message },
                };
            })
        );
        diagnostics_.append_range(diagnostics);
        return cursor_.make_token(result.kind, start, cursor_.offset(), result.flags);
    }

    diagnostic_collector diagnostics_{};
    lexer_cursor cursor_{};
    bool has_peeked_{};
    token peeked_{};
    bool at_line_start_{ true };
};

/// @brief 一次词法处理的完整输出。
export struct lex_result
{
    std::vector<token> tokens{};
    std::vector<diagnostic> diagnostics{};
};

/// @brief 对指定预处理文件执行完整词法处理。
export auto lex(preprocessed_file const& file) -> lex_result
{
    auto state = lexer{ file };
    auto tokens = state.tokenize_all();
    return lex_result {
        .tokens = std::move(tokens),
        .diagnostics = state.take_diagnostics(),
    };
}
