export module lexer.token;

import std;
import lexer.source;

/// 描述词法分析器能够产出的所有 token 类型。
export enum class token_kind
{
    /// 输入结束。
    eof,
    /// 无效或无法识别的输入片段。
    invalid,

    /// 标识符。
    identifier,
    /// 整数字面量。
    integer_literal,
    /// 浮点数字面量。
    float_literal,
    /// 字符字面量。
    char_literal,
    /// 字符串字面量。
    string_literal,

    /// `let` 关键字。
    kw_let,
    /// `const` 关键字。
    kw_const,
    /// `if` 关键字。
    kw_if,
    /// `else` 关键字。
    kw_else,
    /// `while` 关键字。
    kw_while,
    /// `do` 关键字。
    kw_do,
    /// `for` 关键字。
    kw_for,
    /// `break` 关键字。
    kw_break,
    /// `continue` 关键字。
    kw_continue,
    /// `return` 关键字。
    kw_return,
    /// `import` 关键字。
    kw_import,
    /// `export` 关键字。
    kw_export,
    /// `module` 关键字。
    kw_module,
    /// `struct` 关键字。
    kw_struct,
    /// `impl` 关键字。
    kw_impl,
    /// `trait` 关键字。
    kw_trait,
    /// `as` 关键字。
    kw_as,
    /// `true` 关键字。
    kw_true,
    /// `false` 关键字。
    kw_false,
    /// `and` 关键字。
    kw_and,
    /// `or` 关键字。
    kw_or,
    /// `not` 关键字。
    kw_not,

    /// 左圆括号 `(`。
    l_paren,
    /// 右圆括号 `)`。
    r_paren,
    /// 左花括号 `{`。
    l_brace,
    /// 右花括号 `}`。
    r_brace,
    /// 左方括号 `[`。
    l_bracket,
    /// 右方括号 `]`。
    r_bracket,
    /// 逗号 `,`。
    comma,
    /// 分号 `;`。
    semicolon,
    /// 冒号 `:`。
    colon,
    /// 双冒号 `::`。
    colon_colon,
    /// 点号 `.`。
    dot,
    /// 箭头 `->`。
    arrow,
    /// 加号 `+`。
    plus,
    /// 加等号 `+=`。
    plus_equal,
    /// 减号 `-`。
    minus,
    /// 减等号 `-=`。
    minus_equal,
    /// 星号 `*`。
    star,
    /// 乘等号 `*=`。
    star_equal,
    /// 斜杠 `/`。
    slash,
    /// 除等号 `/=`。
    slash_equal,
    /// 百分号 `%`。
    percent,
    /// 取模等号 `%=`。
    percent_equal,
    /// 赋值号 `=`。
    equal,
    /// 相等比较 `==`。
    equal_equal,
    /// 不等比较 `!=`。
    bang_equal,
    /// 小于号 `<`。
    less,
    /// 小于等于 `<=`。
    less_equal,
    /// 大于号 `>`。
    greater,
    /// 大于等于 `>=`。
    greater_equal,
    /// 按位与 `&`。
    amp,
    /// 按位与赋值 `&=`。
    amp_equal,
    /// 按位或 `|`。
    pipe,
    /// 按位或赋值 `|=`。
    pipe_equal,
    /// 按位异或 `^`。
    caret,
    /// 按位异或赋值 `^=`。
    caret_equal,
    /// 波浪号 `~`。
    tilde,
    /// 左移 `<<`。
    less_less,
    /// 左移赋值 `<<=`。
    less_less_equal,
    /// 右移 `>>`。
    greater_greater,
    /// 右移赋值 `>>=`。
    greater_greater_equal,
    /// 自增 `++`。
    plus_plus,
    /// 自减 `--`。
    minus_minus,
    /// 问号 `?`。
    question,
};

/// 将 `token_kind` 转为可读的稳定字符串名。
///
/// @param kind 要转换的 token 类型。
/// @return 与枚举值对应的字符串名；未知值返回 `"unknown"`。
export [[nodiscard]] auto to_string(token_kind kind) -> std::string_view;

/// 描述 token 在扫描过程中附加的状态标记。
export enum class token_flags : std::uint8_t
{
    /// 没有附加标记。
    none = 0,
    /// 该 token 前存在空白。
    leading_space = 1 << 0,
    /// 该 token 位于行首。
    start_of_line = 1 << 1,
    /// 该 token 对应的输入片段未闭合。
    unterminated = 1 << 2,
    /// 该 token 是错误恢复过程中生成的。
    recovered = 1 << 3,
};

/// 计算两个 token 标记集合的并集。
///
/// @param lhs 左操作数标记集合。
/// @param rhs 右操作数标记集合。
/// @return 合并后的标记集合。
export [[nodiscard]] constexpr auto operator|(token_flags lhs, token_flags rhs) -> token_flags
{
    return static_cast<token_flags>(
        static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

/// 将右侧标记并入左侧标记集合。
///
/// @param lhs 需要被更新的标记集合。
/// @param rhs 要并入的标记集合。
/// @return 更新后的 `lhs`。
export constexpr auto operator|=(token_flags& lhs, token_flags rhs) -> token_flags&
{
    lhs = lhs | rhs;
    return lhs;
}

/// 判断某个标记位是否已设置。
///
/// @param flags 待检查的标记集合。
/// @param bit 需要判断的单个标记位。
/// @return 若 `flags` 中包含 `bit`，则返回 `true`，否则返回 `false`。
export [[nodiscard]] constexpr auto has_flag(token_flags flags, token_flags bit) -> bool
{
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(bit)) != 0;
}

/// 表示词法分析器输出的一个 token。
export struct token
{
    /// 比较两个 token 是否完全相等。
    ///
    /// @param other 待比较的 token。
    /// @return 若两个 token 的所有字段都相同，则返回 `true`。
    [[nodiscard]] constexpr auto operator==(token const& other) const -> bool = default;

    /// token 的种类。
    token_kind kind{};
    /// token 在原始源码中的区间。
    span source_span{};
    /// token 的附加标记。
    token_flags flags{token_flags::none};
};

auto to_string(token_kind kind) -> std::string_view
{
    using enum token_kind;

    switch (kind) {
    case eof: return "eof";
    case invalid: return "invalid";
    case identifier: return "identifier";
    case integer_literal: return "integer_literal";
    case float_literal: return "float_literal";
    case char_literal: return "char_literal";
    case string_literal: return "string_literal";
    case kw_let: return "kw_let";
    case kw_const: return "kw_const";
    case kw_if: return "kw_if";
    case kw_else: return "kw_else";
    case kw_while: return "kw_while";
    case kw_do: return "kw_do";
    case kw_for: return "kw_for";
    case kw_break: return "kw_break";
    case kw_continue: return "kw_continue";
    case kw_return: return "kw_return";
    case kw_import: return "kw_import";
    case kw_export: return "kw_export";
    case kw_module: return "kw_module";
    case kw_struct: return "kw_struct";
    case kw_impl: return "kw_impl";
    case kw_trait: return "kw_trait";
    case kw_as: return "kw_as";
    case kw_true: return "kw_true";
    case kw_false: return "kw_false";
    case kw_and: return "kw_and";
    case kw_or: return "kw_or";
    case kw_not: return "kw_not";
    case l_paren: return "l_paren";
    case r_paren: return "r_paren";
    case l_brace: return "l_brace";
    case r_brace: return "r_brace";
    case l_bracket: return "l_bracket";
    case r_bracket: return "r_bracket";
    case comma: return "comma";
    case semicolon: return "semicolon";
    case colon: return "colon";
    case colon_colon: return "colon_colon";
    case dot: return "dot";
    case arrow: return "arrow";
    case plus: return "plus";
    case plus_equal: return "plus_equal";
    case minus: return "minus";
    case minus_equal: return "minus_equal";
    case star: return "star";
    case star_equal: return "star_equal";
    case slash: return "slash";
    case slash_equal: return "slash_equal";
    case percent: return "percent";
    case percent_equal: return "percent_equal";
    case equal: return "equal";
    case equal_equal: return "equal_equal";
    case bang_equal: return "bang_equal";
    case less: return "less";
    case less_equal: return "less_equal";
    case greater: return "greater";
    case greater_equal: return "greater_equal";
    case amp: return "amp";
    case amp_equal: return "amp_equal";
    case pipe: return "pipe";
    case pipe_equal: return "pipe_equal";
    case caret: return "caret";
    case caret_equal: return "caret_equal";
    case tilde: return "tilde";
    case less_less: return "less_less";
    case less_less_equal: return "less_less_equal";
    case greater_greater: return "greater_greater";
    case greater_greater_equal: return "greater_greater_equal";
    case plus_plus: return "plus_plus";
    case minus_minus: return "minus_minus";
    case question: return "question";
    }

    return "unknown";
}
