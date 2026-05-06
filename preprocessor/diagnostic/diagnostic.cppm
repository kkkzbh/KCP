export module preprocessor.diagnostic;

import std;
import source;

using namespace std::string_view_literals;

/// @brief 表示预处理阶段可能报告的诊断类别。
/// @details 当前阶段只区分会破坏后续扫描的结构性问题；与词法诊断相比粒度更粗。
export enum class preprocess_diagnostic_kind
{
    unterminated_block_comment, ///< 块注释没有正确结束。
};

/// @brief 描述一次预处理诊断的完整载荷。
/// @details 该结构保存诊断类别和受影响的源码区间，方便后续阶段在同一偏移重放诊断。
export struct preprocess_diagnostic
{
    /// @brief 判断两个预处理诊断是否表示同一条记录。
    /// @param other 待比较的诊断。
    /// @return 若两者的类别和源范围都相同，则返回 `true`。
    auto constexpr operator==(preprocess_diagnostic const&) const -> bool = default;

    preprocess_diagnostic_kind kind{}; ///< 诊断的具体类别。
    source_span span{};                ///< 诊断对应的原始源码区间。
};

/// @brief 将 `preprocess_diagnostic_kind` 转为稳定的可读字符串名。
/// @param kind 要转换的诊断类别。
/// @return 与枚举值对应的字符串名；未知值返回 `"unknown"`。
export auto to_string(preprocess_diagnostic_kind kind) -> std::string_view
{
    using enum preprocess_diagnostic_kind;

    switch(kind) {
        case unterminated_block_comment: return "unterminated_block_comment"sv;
    }

    return "unknown"sv;
}

/// @brief 兼容旧命名的别名：lexer 与测试代码仍以 `preprocess_issue*` 书写。
export using preprocess_issue_kind = preprocess_diagnostic_kind;
/// @copydoc preprocess_issue_kind
export using preprocess_issue      = preprocess_diagnostic;
