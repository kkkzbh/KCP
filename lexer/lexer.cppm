/// @brief `lexer` 模块的公共入口。
/// @details 该聚合模块统一转发词法分析相关的基础类型、字符分类、诊断设施与扫描器实现。
export module lexer;

/// @brief 源码位置与源文本切片支持。
export import source;
/// @brief 词法单元类型、标志、字符串名与相等比较支持。
export import lexer.token;
/// @brief 词法分析诊断数据结构与接收器。
export import lexer.diagnostic;
/// @brief 字符分类原语。
export import lexer.charset;
/// @brief 词法分析器实现。
export import lexer.scanner;
