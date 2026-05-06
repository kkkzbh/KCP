/// @brief 词法分析器公共 facade。
/// @details 对外保持 `lexer{sources, file, sink}` 用法；内部把不同 sink 适配为
///          `diagnostic_reporter`，并委托给非模板 `scanner_engine`。
export module lexer.scanner;

import std;
import source;
import lexer.token;
import lexer.diagnostic;
import lexer.diagnostic_reporter;
import lexer.scanner_engine;

/// @brief 词法分析器，负责把预处理后的源文本切分为 token 序列。
export struct lexer
{
    /// @brief 构造词法分析器，并立即装载指定文件的预处理结果。
    template<diagnostic_sink Sink>
    lexer(source_manager const& sources, file_id file, Sink& sink)
        : engine_(sources, file, make_diagnostic_reporter(sink))
    {
    }

    /// @brief 重新绑定到另一个文件，并清空当前扫描状态。
    auto reset(file_id file) -> void
    {
        engine_.reset(file);
    }

    /// @brief 返回当前 token，但不推进扫描位置。
    auto peek() -> token
    {
        return engine_.peek();
    }

    /// @brief 返回下一个 token，并推进扫描位置。
    auto next() -> token
    {
        return engine_.next();
    }

    /// @brief 反复扫描直到文件结束，并返回全部 token（含末尾 `eof`）。
    auto tokenize_all() -> std::vector<token>
    {
        return engine_.tokenize_all();
    }

private:
    scanner_engine engine_;
};
