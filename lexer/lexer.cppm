/// @brief 词法分析器公共 facade 与扫描状态机实现。
/// @details 对外提供一次性 `lex(preprocessed_file)` 入口；内部直接维护扫描状态和诊断列表。
export module lexer;

import std;
export import lexer.token;
export import diagnostic;
export import lexer.charset;
import preprocessor.preprocessed;
import :state;
import :keyword;
import :punctuation;
import :literal;

lexer::lexer(preprocessed_file const& file)
{
    reset(file);
}

auto lexer::reset(preprocessed_file const& file) -> void
{
    cursor_.reset(file);
    diagnostics_.clear();
    has_peeked_ = false;
    peeked_ = {};
    at_line_start_ = true;
}

auto lexer::take_diagnostics() -> std::vector<diagnostic>
{
    return diagnostics_.take();
}

auto lexer::peek() -> token
{
    if(not has_peeked_) {
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

    if(cursor_.eof()) {
        return cursor_.make_token(token_kind::eof, cursor_.offset(), cursor_.offset(), flags);
    }

    return lex_token(flags);
}

auto lexer::tokenize_all() -> std::vector<token>
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

auto lexer::skip_trivia() -> trivia_state
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

auto lexer::lex_token(token_flags flags) -> token
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

auto lexer::make_punctuation_token(
    token_kind kind,
    std::size_t start,
    std::size_t length,
    token_flags flags) -> token
{
    cursor_.advance(length);
    return cursor_.make_token(kind, start, cursor_.offset(), flags);
}

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
