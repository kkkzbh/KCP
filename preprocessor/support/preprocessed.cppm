export module preprocessor.preprocessed;

import std;
import preprocessor.diagnostic;

/// @brief 预处理阶段生成的规范化源码与诊断列表。
/// @details `normalized_text` 与原始源码长度一致，后续阶段可直接复用原始偏移；
/// `issues` 按诊断起始偏移递增排列，方便词法阶段顺序消费。
export struct preprocessed_file
{
    /// @brief 查询给定偏移是否正好位于某条诊断记录的起点。
    /// @param offset 要查询的源码偏移。
    /// @return 起始偏移等于 `offset` 的诊断指针；若没有则返回 `nullptr`。
    auto issue_at(std::size_t offset) const -> preprocess_diagnostic const*
    {
        auto const it = std::ranges::find(issues, offset, [](preprocess_diagnostic const& issue) {
            return issue.source_span.start;
        });
        return it == issues.end() ? nullptr : &*it;
    }

    std::string normalized_text;               ///< 规范化后的源文本，长度与原始源码一致。
    std::vector<preprocess_diagnostic> issues; ///< 预处理过程中遇到的诊断列表。
};
