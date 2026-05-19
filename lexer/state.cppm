/// @brief 词法扫描状态机的内部声明。
module lexer:state;

import std;
import diagnostic;
import preprocessor.preprocessed;
import lexer.cursor;
import lexer.token;

/// @brief 词法扫描状态机，负责把预处理后的源文本切分为 token 序列。
struct lexer
{
    /// @brief 构造词法分析器，并立即装载指定文件的预处理结果。
    lexer(preprocessed_file const& file);

    /// @brief 重新绑定到另一个预处理文件，并清空当前扫描状态。
    auto reset(preprocessed_file const& file) -> void;

    /// @brief 取出当前扫描产生的词法诊断。
    auto take_diagnostics() -> std::vector<diagnostic>;

    /// @brief 返回当前 token，但不推进扫描位置。
    auto peek() -> token;

    /// @brief 返回下一个 token，并推进扫描位置。
    auto next() -> token;

    /// @brief 反复扫描直到文件结束，并返回全部 token（含末尾 `eof`）。
    auto tokenize_all() -> std::vector<token>;

private:
    struct trivia_state
    {
        bool saw_space{};
        bool at_line_start{ true };
    };

    auto skip_trivia() -> trivia_state;
    auto lex_token(token_flags flags) -> token;
    auto make_punctuation_token(token_kind kind, std::size_t start, std::size_t length, token_flags flags) -> token;
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
