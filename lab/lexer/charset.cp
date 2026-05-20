export module lexer.charset;

export is_space(ch: char) -> bool
{
    return ch == ' ' or ch == '\n' or ch == '\t' or ch == '\r';
}

export is_alpha(ch: char) -> bool
{
    return (ch >= 'a' and ch <= 'z') or (ch >= 'A' and ch <= 'Z');
}

export is_decimal_digit(ch: char) -> bool
{
    return ch >= '0' and ch <= '9';
}

export is_identifier_start(ch: char) -> bool
{
    return is_alpha(ch) or ch == '_';
}

export is_identifier_continue(ch: char) -> bool
{
    return is_identifier_start(ch) or is_decimal_digit(ch);
}
