/// @brief `preprocessor` 模块的公共入口。
/// @details 该聚合模块统一转发预处理阶段的问题类型、产出结构与扫描器实现。
export module preprocessor;

/// @brief 预处理问题类型与转换支持。
export import preprocessor.issue;
/// @brief 预处理产出结构。
export import preprocessor.output;
/// @brief 预处理扫描器实现。
export import preprocessor.scanner;
