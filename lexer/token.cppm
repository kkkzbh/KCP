export module lexer.token;

import std;
import source;

/// @brief 描述词法分析器能够产出的所有 token 类型。
/// @details 这些枚举值既用于词法阶段输出，也作为后续语法分析阶段的稳定终结符集合。
export enum class token_kind
{
    eof,     ///< 输入结束。
    invalid, ///< 无效或无法识别的输入片段。

    identifier,      ///< 标识符。
    integer_literal, ///< 整数字面量。
    float_literal,   ///< 浮点数字面量。
    char_literal,    ///< 字符字面量。
    string_literal,  ///< 字符串字面量。

    kw_let,      ///< `let` 关键字。
    kw_const,    ///< `const` 关键字。
    kw_if,       ///< `if` 关键字。
    kw_else,     ///< `else` 关键字。
    kw_while,    ///< `while` 关键字。
    kw_do,       ///< `do` 关键字。
    kw_for,      ///< `for` 关键字。
    kw_break,    ///< `break` 关键字。
    kw_continue, ///< `continue` 关键字。
    kw_return,   ///< `return` 关键字。
    kw_import,   ///< `import` 关键字。
    kw_export,   ///< `export` 关键字。
    kw_module,   ///< `module` 关键字。
    kw_struct,   ///< `struct` 关键字。
    kw_enum,     ///< `enum` 关键字。
    kw_impl,     ///< `impl` 关键字。
    kw_concept,  ///< `concept` 关键字。
    kw_operator, ///< `operator` 关键字。
    kw_as,       ///< `as` 关键字。
    kw_true,     ///< `true` 关键字。
    kw_false,    ///< `false` 关键字。
    kw_nullptr,  ///< `nullptr` 关键字。
    kw_and,      ///< `and` 关键字。
    kw_or,       ///< `or` 关键字。
    kw_not,      ///< `not` 关键字。
    kw_ref,      ///< `ref` 关键字。
    kw_move,     ///< `move` 关键字。
    kw_like,     ///< `like` 关键字。
    kw_forward,  ///< `forward` 关键字。
    kw_new,      ///< `new` 关键字。
    kw_delete,   ///< `delete` 关键字。

    l_paren,               ///< 左圆括号 `(`。
    r_paren,               ///< 右圆括号 `)`。
    l_brace,               ///< 左花括号 `{`。
    r_brace,               ///< 右花括号 `}`。
    l_bracket,             ///< 左方括号 `[`。
    r_bracket,             ///< 右方括号 `]`。
    comma,                 ///< 逗号 `,`。
    semicolon,             ///< 分号 `;`。
    colon,                 ///< 冒号 `:`。
    colon_colon,           ///< 双冒号 `::`。
    dot,                   ///< 点号 `.`。
    arrow,                 ///< 箭头 `->`。
    plus,                  ///< 加号 `+`。
    plus_equal,            ///< 加等号 `+=`。
    minus,                 ///< 减号 `-`。
    minus_equal,           ///< 减等号 `-=`。
    star,                  ///< 星号 `*`。
    star_equal,            ///< 乘等号 `*=`。
    slash,                 ///< 斜杠 `/`。
    slash_equal,           ///< 除等号 `/=`。
    percent,               ///< 百分号 `%`。
    percent_equal,         ///< 取模等号 `%=`。
    equal,                 ///< 赋值号 `=`。
    equal_equal,           ///< 相等比较 `==`。
    bang,                  ///< never type `!`。
    bang_equal,            ///< 不等比较 `!=`。
    less,                  ///< 小于号 `<`。
    less_equal,            ///< 小于等于 `<=`。
    spaceship,             ///< 三路比较 `<=>`。
    greater,               ///< 大于号 `>`。
    greater_equal,         ///< 大于等于 `>=`。
    amp,                   ///< 按位与 `&`。
    amp_equal,             ///< 按位与赋值 `&=`。
    pipe,                  ///< 按位或 `|`。
    pipe_equal,            ///< 按位或赋值 `|=`。
    caret,                 ///< 按位异或 `^`。
    caret_equal,           ///< 按位异或赋值 `^=`。
    tilde,                 ///< 波浪号 `~`。
    less_less,             ///< 左移 `<<`。
    less_less_equal,       ///< 左移赋值 `<<=`。
    greater_greater,       ///< 右移 `>>`。
    greater_greater_equal, ///< 右移赋值 `>>=`。
    plus_plus,             ///< 自增 `++`。
    minus_minus,           ///< 自减 `--`。
    question,              ///< 问号 `?`。
};

/// @brief `token_kind` 与其字符串名的对照表。
/// @details 表项顺序与 `token_kind` 枚举底层值一致；新增枚举值时需同步更新。
export auto constexpr token_name_table = std::to_array<std::pair<token_kind, std::string_view>>({
    { token_kind::eof, "eof" },
    { token_kind::invalid, "invalid" },
    { token_kind::identifier, "identifier" },
    { token_kind::integer_literal, "integer_literal" },
    { token_kind::float_literal, "float_literal" },
    { token_kind::char_literal, "char_literal" },
    { token_kind::string_literal, "string_literal" },
    { token_kind::kw_let, "kw_let" },
    { token_kind::kw_const, "kw_const" },
    { token_kind::kw_if, "kw_if" },
    { token_kind::kw_else, "kw_else" },
    { token_kind::kw_while, "kw_while" },
    { token_kind::kw_do, "kw_do" },
    { token_kind::kw_for, "kw_for" },
    { token_kind::kw_break, "kw_break" },
    { token_kind::kw_continue, "kw_continue" },
    { token_kind::kw_return, "kw_return" },
    { token_kind::kw_import, "kw_import" },
    { token_kind::kw_export, "kw_export" },
    { token_kind::kw_module, "kw_module" },
    { token_kind::kw_struct, "kw_struct" },
    { token_kind::kw_enum, "kw_enum" },
    { token_kind::kw_impl, "kw_impl" },
    { token_kind::kw_concept, "kw_concept" },
    { token_kind::kw_operator, "kw_operator" },
    { token_kind::kw_as, "kw_as" },
    { token_kind::kw_true, "kw_true" },
    { token_kind::kw_false, "kw_false" },
    { token_kind::kw_nullptr, "kw_nullptr" },
    { token_kind::kw_and, "kw_and" },
    { token_kind::kw_or, "kw_or" },
    { token_kind::kw_not, "kw_not" },
    { token_kind::kw_ref, "kw_ref" },
    { token_kind::kw_move, "kw_move" },
    { token_kind::kw_like, "kw_like" },
    { token_kind::kw_forward, "kw_forward" },
    { token_kind::kw_new, "kw_new" },
    { token_kind::kw_delete, "kw_delete" },
    { token_kind::l_paren, "l_paren" },
    { token_kind::r_paren, "r_paren" },
    { token_kind::l_brace, "l_brace" },
    { token_kind::r_brace, "r_brace" },
    { token_kind::l_bracket, "l_bracket" },
    { token_kind::r_bracket, "r_bracket" },
    { token_kind::comma, "comma" },
    { token_kind::semicolon, "semicolon" },
    { token_kind::colon, "colon" },
    { token_kind::colon_colon, "colon_colon" },
    { token_kind::dot, "dot" },
    { token_kind::arrow, "arrow" },
    { token_kind::plus, "plus" },
    { token_kind::plus_equal, "plus_equal" },
    { token_kind::minus, "minus" },
    { token_kind::minus_equal, "minus_equal" },
    { token_kind::star, "star" },
    { token_kind::star_equal, "star_equal" },
    { token_kind::slash, "slash" },
    { token_kind::slash_equal, "slash_equal" },
    { token_kind::percent, "percent" },
    { token_kind::percent_equal, "percent_equal" },
    { token_kind::equal, "equal" },
    { token_kind::equal_equal, "equal_equal" },
    { token_kind::bang, "bang" },
    { token_kind::bang_equal, "bang_equal" },
    { token_kind::less, "less" },
    { token_kind::less_equal, "less_equal" },
    { token_kind::spaceship, "spaceship" },
    { token_kind::greater, "greater" },
    { token_kind::greater_equal, "greater_equal" },
    { token_kind::amp, "amp" },
    { token_kind::amp_equal, "amp_equal" },
    { token_kind::pipe, "pipe" },
    { token_kind::pipe_equal, "pipe_equal" },
    { token_kind::caret, "caret" },
    { token_kind::caret_equal, "caret_equal" },
    { token_kind::tilde, "tilde" },
    { token_kind::less_less, "less_less" },
    { token_kind::less_less_equal, "less_less_equal" },
    { token_kind::greater_greater, "greater_greater" },
    { token_kind::greater_greater_equal, "greater_greater_equal" },
    { token_kind::plus_plus, "plus_plus" },
    { token_kind::minus_minus, "minus_minus" },
    { token_kind::question, "question" },
});

/// @brief 将 `token_kind` 转为稳定的可读字符串名。
/// @param kind 要转换的 token 类型。
/// @return 与枚举值对应的字符串名。
export auto to_string(token_kind kind) -> std::string_view
{
    return token_name_table[std::to_underlying(kind)].second;
}

/// @brief 描述 token 在扫描过程中附加的状态标记。
/// @details 这些标记不改变 token 的语义类别，但会影响错误恢复、格式感知和后续工具行为。
export enum class token_flags : std::uint8_t
{
    none = 0,              ///< 没有附加标记。
    leading_space = 1 << 0,///< 该 token 前存在空白。
    start_of_line = 1 << 1,///< 该 token 位于行首。
    unterminated = 1 << 2, ///< 该 token 对应的输入片段未闭合。
    recovered = 1 << 3,    ///< 该 token 是错误恢复过程中生成的。
};

/// @brief 计算两个 token 标记集合的并集。
/// @param lhs 左操作数标记集合。
/// @param rhs 右操作数标记集合。
/// @return 合并后的标记集合。
export auto constexpr operator|(token_flags lhs, token_flags rhs) -> token_flags
{
    return static_cast<token_flags> (
        static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs)
    );
}

/// @brief 将右侧标记并入左侧标记集合。
/// @param lhs 需要被更新的标记集合。
/// @param rhs 要并入的标记集合。
/// @return 更新后的 `lhs`。
export auto constexpr operator|=(token_flags& lhs, token_flags rhs) -> token_flags&
{
    lhs = lhs | rhs;
    return lhs;
}

/// @brief 判断某个标记位是否已设置。
/// @param flags 待检查的标记集合。
/// @param bit 需要判断的单个标记位。
/// @return 若 `flags` 中包含 `bit`，则返回 `true`，否则返回 `false`。
export auto constexpr has_flag(token_flags flags, token_flags bit) -> bool
{
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(bit)) != 0;
}

/// @brief 表示词法分析器输出的一个 token。
/// @details 该结构同时保存 token 种类、原始源码位置和扫描期附带的元信息。
export struct token
{
    /// @brief 比较两个 token 是否完全相等。
    /// @param other 待比较的 token。
    /// @return 若两个 token 的所有字段都相同，则返回 `true`。
    auto constexpr operator==(token const& other) const -> bool = default;

    token_kind kind{};                        ///< token 的种类。
    source_span span{};                       ///< token 在原始源码中的区间。
    std::string text{};                       ///< 标识符等上下文敏感 token 的源码文本。
    token_flags flags{ token_flags::none };     ///< token 的附加标记。
};
