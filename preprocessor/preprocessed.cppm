export module preprocessor.preprocessed;

import std;
import source;
import diagnostic;

/// @brief 预处理阶段生成的规范化源码与诊断列表。
/// @details `normalized_text` 与原始源码长度一致，后续阶段可直接复用原始偏移；
/// `diagnostics` 按诊断起始偏移递增排列，且 span 活在全局地址空间中。
export struct preprocessed_file
{
    std::string normalized_text; ///< 规范化后的源文本，长度与原始源码一致。
    std::vector<diagnostic> diagnostics; ///< 预处理诊断列表。
    byte_pos file_start{};       ///< 所属文件在全局地址空间中的起点。
};
