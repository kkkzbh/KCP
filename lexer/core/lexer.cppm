/// `lexer` 模块的公共入口，负责统一转发词法分析相关子模块。
export module lexer;

/// 源码位置与源文本切片支持。
export import lexer.source;
/// 词法单元类型、标志和文本转换支持。
export import lexer.token;
/// 词法分析诊断支持。
export import lexer.diagnostic;
/// 词法分析器实现。
export import lexer.scanner;
