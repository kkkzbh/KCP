export module preprocessor.preprocessed;

import std;
import source;
import preprocessor.diagnostic;

/// @brief 预处理阶段生成的规范化源码与诊断列表。
/// @details `normalized_text` 与原始源码长度一致，后续阶段可直接复用原始偏移；
/// `issues` 按诊断起始偏移递增排列，方便词法阶段顺序消费。
/// @note `issues[i].span` 活在全局地址空间中；为方便单文件视角下按文件内偏移查询，
///       此结构同时记录所属文件在全局空间的起点 `file_start`。
export struct preprocessed_file
{
    /// @brief 查询给定文件内偏移是否正好位于某条诊断记录的起点。
    /// @param offset 文件内偏移。
    auto issue_at(std::size_t offset) const -> preprocess_diagnostic const*
    {
        auto const target = file_start + static_cast<byte_pos>(offset);
        auto const it = std::ranges::find(issues, target, [](preprocess_diagnostic const& issue) {
            return issue.span.start;
        });
        return it == issues.end() ? nullptr : &*it;
    }

    std::string normalized_text;               ///< 规范化后的源文本，长度与原始源码一致。
    std::vector<preprocess_diagnostic> issues; ///< 预处理过程中遇到的诊断列表。
    byte_pos file_start{};                     ///< 所属文件在全局地址空间中的起点。
};
