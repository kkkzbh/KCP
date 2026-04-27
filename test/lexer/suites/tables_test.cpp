import std;
import lexer;

#include "assert.hpp"

auto main() -> int
{
    // 最长匹配：`<<=` 应优先于 `<<` 与 `<`
    {
        auto const hit = match_punctuation("<<=x");
        test_lexer::assert_true(hit.has_value(), "match_punctuation should match <<=");
        test_lexer::assert_true(hit->first == token_kind::less_less_equal,
            "<<= should map to less_less_equal");
        test_lexer::assert_true(hit->second == 3, "<<= should consume 3 chars");
    }
    {
        auto const hit = match_punctuation("<<y");
        test_lexer::assert_true(hit.has_value() and hit->first == token_kind::less_less,
            "<< should map to less_less");
        test_lexer::assert_true(hit->second == 2, "<< should consume 2 chars");
    }
    {
        auto const hit = match_punctuation("<y");
        test_lexer::assert_true(hit.has_value() and hit->first == token_kind::less,
            "< should map to less");
        test_lexer::assert_true(hit->second == 1, "< should consume 1 char");
    }
    {
        auto const hit = match_punctuation("@");
        test_lexer::assert_true(not hit.has_value(), "non-punctuation should miss");
    }

    // keyword_kind：命中关键字，但前缀式拼写不命中
    {
        auto const hit = keyword_kind("if");
        test_lexer::assert_true(hit.has_value() and *hit == token_kind::kw_if,
            "if should map to kw_if");
    }
    {
        auto const hit = keyword_kind("ifx");
        test_lexer::assert_true(not hit.has_value(), "ifx should not be a keyword");
    }
    {
        auto const hit = keyword_kind("");
        test_lexer::assert_true(not hit.has_value(), "empty text should miss");
    }

    // to_string：每个 token_kind 都有稳定的非空名字，且没有 unknown
    for (auto const& [kind, name] : token_name_table) {
        test_lexer::assert_true(not name.empty(), "token name should be non-empty");
        test_lexer::assert_true(to_string(kind) == name,
            "to_string(kind) should agree with token_name_table");
        test_lexer::assert_true(name != "unknown",
            "no declared token_kind should stringify to 'unknown'");
    }

    // 字符分类：覆盖若干关键分支
    test_lexer::assert_true(is_identifier_start('_'), "'_' starts identifier");
    test_lexer::assert_true(is_identifier_start('a'), "'a' starts identifier");
    test_lexer::assert_true(not is_identifier_start('1'), "'1' does not start identifier");
    test_lexer::assert_true(is_identifier_continue('9'), "'9' continues identifier");
    test_lexer::assert_true(is_decimal_digit('0') and is_decimal_digit('9'), "decimal digit range");
    test_lexer::assert_true(is_octal_digit('7') and not is_octal_digit('8'), "octal digit range");
    test_lexer::assert_true(is_hex_digit('f') and is_hex_digit('F') and not is_hex_digit('g'),
        "hex digit recognition");
    test_lexer::assert_true(is_exponent_marker('e') and is_exponent_marker('E'),
        "exponent marker recognition");
    test_lexer::assert_true(is_recovery_delimiter(';') and is_recovery_delimiter(' '),
        "recovery delimiter recognition");
    test_lexer::assert_true(not is_recovery_delimiter('a'), "letters are not recovery delimiters");
    test_lexer::assert_true(is_simple_escape('n') and is_simple_escape('\\'),
        "simple escape recognition");
    test_lexer::assert_true(not is_simple_escape('z'), "unsupported escape is not simple");

    return 0;
}
