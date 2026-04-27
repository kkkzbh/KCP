/// @brief 词法分析使用的字符分类原语。
/// @details 全部按 ASCII 语义实现，避免 `<cctype>` 的 locale 依赖与符号性陷阱，并支持编译期求值。
export module lexer.charset;

import std;

using namespace std::literals;

/// @brief 判断字符是否可作为标识符起始。
export auto constexpr is_identifier_start(char ch) -> bool
{
    return ch == '_'
        or (ch >= 'a' and ch <= 'z')
        or (ch >= 'A' and ch <= 'Z');
}

/// @brief 判断字符是否是十进制数字。
export auto constexpr is_decimal_digit(char ch) -> bool
{
    return ch >= '0' and ch <= '9';
}

/// @brief 判断字符是否可出现在标识符非起始位置。
export auto constexpr is_identifier_continue(char ch) -> bool
{
    return is_identifier_start(ch) or is_decimal_digit(ch);
}

/// @brief 判断字符是否是八进制数字。
export auto constexpr is_octal_digit(char ch) -> bool
{
    return ch >= '0' and ch <= '7';
}

/// @brief 判断字符是否是十六进制数字。
export auto constexpr is_hex_digit(char ch) -> bool
{
    return is_decimal_digit(ch)
        or (ch >= 'a' and ch <= 'f')
        or (ch >= 'A' and ch <= 'F');
}

/// @brief 判断字符是否为指数记号（`e`/`E`）。
export auto constexpr is_exponent_marker(char ch) -> bool
{
    return ch == 'e' or ch == 'E';
}

/// @brief 判断字符是否可以终止错误恢复过程中的"吞噬"。
/// @details 遇到这些字符时，恢复逻辑会停止向前吞噬当前无效片段。
export auto constexpr is_recovery_delimiter(char ch) -> bool
{
    auto constexpr delimiters = " \t\r\n(){}[],:;.+-*/%<>=&|^~?\"'"sv;
    return delimiters.contains(ch);
}

/// @brief 判断字符是否是可直接通过的简单转义字符。
export auto constexpr is_simple_escape(char ch) -> bool
{
    auto constexpr escapes = "\\'\"?abfnrtv"sv;
    return escapes.contains(ch);
}

/// @brief 判断字符是否为 ASCII 空白字符。
export auto constexpr is_space(char ch) -> bool
{
    return ch == ' ' or ch == '\t' or ch == '\n'
        or ch == '\r' or ch == '\v' or ch == '\f';
}
