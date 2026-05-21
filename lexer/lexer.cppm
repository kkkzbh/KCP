/// @brief 词法分析器公共 facade。
/// @details 对外提供一次性 `lex(preprocessed_file const&)` 入口。
export module lexer;

import std;
export import lexer.token;
export import diagnostic;
export import lexer.charset;
import preprocessor.preprocessed;
import :state;

/// @brief 一次词法处理的完整输出。
export struct lex_result
{
    std::vector<token> tokens{};
    std::vector<diagnostic> diagnostics{};
};

/// @brief 借用指定预处理文件并执行完整词法处理。
export auto lex(preprocessed_file const& file) -> lex_result
{
    auto state = lexer{ file };
    auto tokens = state.tokenize_all();
    return lex_result {
        .tokens = std::move(tokens),
        .diagnostics = state.take_diagnostics(),
    };
}
