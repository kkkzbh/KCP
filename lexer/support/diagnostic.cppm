export module lexer.diagnostic;

import std;
import lexer.source;

/// 表示诊断消息的严重级别。
export enum class diagnostic_severity
{
    /// 表示错误，通常意味着当前输入无法继续按预期处理。
    error,
    /// 表示警告，表示存在问题但通常不阻止继续处理。
    warning,
};

/// 表示词法阶段可能报告的诊断类别。
export enum class diagnostic_code
{
    /// 表示遇到了无法识别的字符。
    invalid_character,
    /// 表示字符串字面量没有正确结束。
    unterminated_string_literal,
    /// 表示字符字面量没有正确结束。
    unterminated_char_literal,
    /// 表示块注释没有正确结束。
    unterminated_block_comment,
    /// 表示字符字面量的内容不合法。
    invalid_char_literal,
    /// 表示转义序列不合法。
    invalid_escape_sequence,
    /// 表示数字字面量后缀不合法。
    invalid_number_suffix,
};

/// 描述一次词法诊断的完整信息。
export struct diagnostic
{
    /// 诊断的严重级别。
    diagnostic_severity severity{ diagnostic_severity::error };
    /// 诊断的具体类别。
    diagnostic_code code{};
    /// 用于向用户展示的诊断消息。
    std::string message;
    /// 诊断对应的主要源码范围。
    span primary_span{};
};

/// 定义接收词法诊断消息的抽象接口。
export struct diagnostic_sink
{
    /// 销毁诊断接收器基类对象。
    virtual ~diagnostic_sink() = default;
    /// 报告一条诊断消息。
    ///
    /// 参数 `value` 表示需要被接收的诊断内容。
    /// 返回值无。
    virtual auto report(diagnostic value) -> void = 0;
};

/// 将收到的诊断消息保存到 `std::vector` 中的接收器实现。
export struct vector_diagnostic_sink final : diagnostic_sink
{
    /// 将一条诊断消息追加到内部存储。
    ///
    /// 参数 `value` 表示需要保存的诊断内容。
    /// 返回值无。
    auto report(diagnostic value) -> void override;
    /// 获取当前已保存的所有诊断消息。
    ///
    /// 返回值为内部诊断列表的只读引用。
    [[nodiscard]] auto diagnostics() const -> std::vector<diagnostic> const&;
    /// 清空当前保存的所有诊断消息。
    ///
    /// 返回值无。
    auto clear() -> void;

private:
    /// 保存所有已接收的诊断消息。
    std::vector<diagnostic> diagnostics_;
};

auto vector_diagnostic_sink::report(diagnostic value) -> void
{
    diagnostics_.push_back(std::move(value));
}

auto vector_diagnostic_sink::diagnostics() const -> std::vector<diagnostic> const&
{
    return diagnostics_;
}

auto vector_diagnostic_sink::clear() -> void
{
    diagnostics_.clear();
}
