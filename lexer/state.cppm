/// @brief 词法扫描状态机的内部声明。
module lexer:state;

import std;
import diagnostic;
import preprocessor.preprocessed;
import lexer.charset;
import lexer.cursor;
import lexer.token;

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
            return lex_identifier_or_keyword(flags);
        }

        if(is_decimal_digit(ch)) {
            return lex_number_literal(flags);
        }

        if(ch == '"') {
            return lex_string_literal(flags);
        }

        if(ch == '\'') {
            return lex_char_literal(flags);
        }

        if(auto punctuation = lex_punctuation(flags)) {
            return *punctuation;
        }

        cursor_.advance();
        diagnostics_.report(diagnostic_kind::invalid_character, cursor_.make_span(start, cursor_.offset()));
        return cursor_.make_token(token_kind::invalid, start, cursor_.offset(), flags | token_flags::recovered);
    }

    auto lex_identifier_or_keyword(token_flags flags) -> token;
    auto lex_punctuation(token_flags flags) -> std::optional<token>;
    auto lex_number_literal(token_flags flags) -> token;
    auto lex_string_literal(token_flags flags) -> token;
    auto lex_char_literal(token_flags flags) -> token;

    diagnostic_collector diagnostics_{};
    lexer_cursor cursor_{};
    bool has_peeked_{};
    token peeked_{};
    bool at_line_start_{ true };
};
