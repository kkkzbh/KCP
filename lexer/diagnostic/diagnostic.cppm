export module lexer.diagnostic;

import std;
import source;
import source.diagnostic;

/// @brief 表示诊断消息严重级别的枚举。
/// @details 词法阶段目前仅区分会阻止正常继续处理的错误和可保留处理结果的警告。
export enum class lexer_diagnostic_severity
{
    error,   ///< 表示错误，通常意味着当前输入无法继续按预期处理。
    warning, ///< 表示警告，表示存在问题但通常不阻止继续处理。
};

/// @brief 表示词法阶段可能报告的诊断类别。
/// @details 每个枚举值都对应一类稳定的错误标签，便于测试和上层工具做分类处理。
export enum class lexer_diagnostic_code
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
export using lexer_diagnostic = diagnostic<lexer_diagnostic_severity, lexer_diagnostic_code>;
