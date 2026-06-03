import std;
import source;
import preprocessor;
import lexer;

#include "assert.hpp"
#include "case_types.hpp"

namespace {

auto first_token_kind(std::string text) -> token_kind
{
    auto sources = source_manager{};
    auto const file = sources.add_source("classification.lex", std::move(text));
    auto preprocessed = preprocess(sources, file);
    auto result = lex(preprocessed);
    auto const token = result.tokens.front();

    test_lexer::assert_true(result.diagnostics.empty(),
        "classification input should not produce diagnostics");
    return token.kind;
}

} // namespace

auto main() -> int
{
    // 关键字分类通过真实 scanner 行为验证，keyword_kind 保持 scanner 私有。
    for(auto const& [text, kind] : std::to_array<std::pair<std::string_view, token_kind>>({
            {"as", token_kind::kw_as},
            {"do", token_kind::kw_do},
            {"if", token_kind::kw_if},
            {"or", token_kind::kw_or},
            {"and", token_kind::kw_and},
            {"for", token_kind::kw_for},
            {"let", token_kind::kw_let},
            {"new", token_kind::kw_new},
            {"not", token_kind::kw_not},
            {"ref", token_kind::kw_ref},
            {"else", token_kind::kw_else},
            {"enum", token_kind::kw_enum},
            {"impl", token_kind::kw_impl},
            {"like", token_kind::kw_like},
            {"move", token_kind::kw_move},
            {"true", token_kind::kw_true},
            {"break", token_kind::kw_break},
            {"const", token_kind::kw_const},
            {"false", token_kind::kw_false},
            {"concept", token_kind::kw_concept},
            {"forward", token_kind::kw_forward},
            {"delete", token_kind::kw_delete},
            {"nullptr", token_kind::kw_nullptr},
            {"while", token_kind::kw_while},
            {"export", token_kind::kw_export},
            {"import", token_kind::kw_import},
            {"module", token_kind::kw_module},
            {"operator", token_kind::kw_operator},
            {"return", token_kind::kw_return},
            {"struct", token_kind::kw_struct},
            {"continue", token_kind::kw_continue},
        })) {
        test_lexer::assert_true(first_token_kind(std::string{ text } + " ") == kind,
            "keyword text should scan as its keyword token");
    }

    for(auto const text : {"ifx", "return0", "continue_"}) {
        test_lexer::assert_true(first_token_kind(std::string{ text } + " ") == token_kind::identifier,
            "keyword prefix extension should scan as identifier");
    }

    // to_string：合法 token_kind 与名称表按底层值一一对应。
    static_assert(token_name_table.size() == test_lexer::all_token_kinds.size());
    for(auto index = std::size_t{}; index < token_name_table.size(); ++index) {
        auto const [kind, name] = token_name_table[index];
        auto const declared_kind = test_lexer::all_token_kinds[index];

        contract_assert(kind == declared_kind);
        contract_assert(std::to_underlying(kind) == index);
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
