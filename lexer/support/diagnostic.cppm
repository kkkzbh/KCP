export module lexer.diagnostic;

import std;
import lexer.source;

/// @brief 表示诊断消息严重级别的枚举。
/// @details 词法阶段目前仅区分会阻止正常继续处理的错误和可保留处理结果的警告。
export enum class diagnostic_severity
{
    error,   ///< 表示错误，通常意味着当前输入无法继续按预期处理。
    warning, ///< 表示警告，表示存在问题但通常不阻止继续处理。
};

/// @brief 表示词法阶段可能报告的诊断类别。
/// @details 每个枚举值都对应一类稳定的错误标签，便于测试和上层工具做分类处理。
export enum class diagnostic_code
{
    invalid_character,             ///< 遇到了无法识别的字符。
    unterminated_string_literal,   ///< 字符串字面量没有正确结束。
    unterminated_char_literal,     ///< 字符字面量没有正确结束。
    unterminated_block_comment,    ///< 块注释没有正确结束。
    invalid_char_literal,          ///< 字符字面量内容不合法。
    invalid_escape_sequence,       ///< 转义序列不合法。
    invalid_number_suffix,         ///< 数字字面量后缀不合法。
};

/// @brief 描述一次词法诊断的完整载荷。
/// @details 该结构同时携带严重级别、诊断分类、展示消息和主要定位区间。
export struct diagnostic
{
    diagnostic_severity severity{ diagnostic_severity::error }; ///< 诊断的严重级别。
    diagnostic_code code{};                                    ///< 诊断的具体类别。
    std::string message;                                       ///< 用于向用户展示的诊断消息。
    span primary_span{};                                       ///< 诊断对应的主要源码范围。
};

/// @brief 约束可作为词法诊断接收器使用的类型。
/// @tparam Sink 候选接收器类型。
/// @details 满足该概念的类型必须提供 `report(diagnostic)`，并以 `void` 作为返回值。
export template<typename Sink>
concept diagnostic_sink = requires(Sink& sink)
{
    { sink.report(diagnostic{}) } -> std::same_as<void>;
};

/// @brief 将收到的诊断消息顺序保存到 `std::vector` 中的默认接收器。
/// @details 主要用于测试、批量收集和稍后统一展示。
export struct vector_diagnostic_sink
{
    auto report(diagnostic value) -> void { diagnostics_.push_back(std::move(value)); }
    auto diagnostics() const -> std::vector<diagnostic> const& { return diagnostics_; }
    auto clear() -> void { diagnostics_.clear(); }

private:
    std::vector<diagnostic> diagnostics_;
};
