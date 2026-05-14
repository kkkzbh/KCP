import std;
import lexer;

#include "assert.hpp"

auto main() -> int
{
    auto sources = source_manager{};
    auto diagnostics = std::vector<lexer_diagnostic>{};
    auto const first = sources.add_source("peek.lex", "let value = 1;");
    auto const second = sources.add_source("reset.lex", "return value;");

    auto lex = lexer{ sources, first, diagnostics };

    auto const peeked_once = lex.peek();
    auto const peeked_twice = lex.peek();
    test_lexer::assert_true(peeked_once == peeked_twice, "peek should be stable");
    test_lexer::assert_true(peeked_once.kind == token_kind::kw_let, "peek should read first token");
    test_lexer::assert_true(has_flag(peeked_once.flags, token_flags::start_of_line),
        "first token should be marked start_of_line");

    auto const consumed_after_peek = lex.next();
    test_lexer::assert_true(consumed_after_peek == peeked_once, "next should consume cached peek token");

    auto const second_token = lex.next();
    test_lexer::assert_true(second_token.kind == token_kind::identifier, "second token should be identifier");
    test_lexer::assert_true(std::string{sources.slice(second_token.span)} == "value",
        "identifier lexeme should match");
    test_lexer::assert_true(has_flag(second_token.flags, token_flags::leading_space),
        "identifier after whitespace should record leading_space");

    lex.reset(second);
    auto const reset_first = lex.peek();
    test_lexer::assert_true(reset_first.kind == token_kind::kw_return, "reset should restart from new file");
    test_lexer::assert_true(has_flag(reset_first.flags, token_flags::start_of_line),
        "reset should restore line-start state");
    test_lexer::assert_true(std::string{sources.slice(reset_first.span)} == "return",
        "reset token span should point to new file text");

    auto const after_reset = lex.next();
    test_lexer::assert_true(after_reset == reset_first, "cached token should survive after reset");

    auto dangling_escape_text = std::string{};
    dangling_escape_text.push_back('\'');
    dangling_escape_text.push_back('\\');
    auto const dangling_escape = sources.add_source("dangling_escape_runtime.lex", dangling_escape_text);
    lex.reset(dangling_escape);

    auto const dangling_token = lex.next();
    test_lexer::assert_true(dangling_token.kind == token_kind::invalid,
        "dangling escape should produce invalid token");
    test_lexer::assert_true(has_flag(dangling_token.flags, token_flags::unterminated),
        "dangling escape should be unterminated");
    test_lexer::assert_true (
        diagnostics.back().code == lexer_diagnostic_code::unterminated_char_literal,
        "dangling escape should diagnose unterminated char");

    auto const unterminated_block = sources.add_source("unterminated_block_runtime.lex", "/* block\ncomment");
    lex.reset(unterminated_block);

    auto const block_token = lex.next();
    test_lexer::assert_true(block_token.kind == token_kind::invalid,
        "unterminated block comment should produce invalid token");
    test_lexer::assert_true(has_flag(block_token.flags, token_flags::unterminated),
        "unterminated block comment should be unterminated");
    test_lexer::assert_true (
        diagnostics.back().code == lexer_diagnostic_code::unterminated_block_comment,
        "unterminated block comment should diagnose correctly");

    auto const comment_span = sources.add_source("comment_span.lex", "let/* inner */value;");
    lex.reset(comment_span);

    auto const first_after_comment = lex.next();
    auto const second_after_comment = lex.next();
    test_lexer::assert_true(first_after_comment.kind == token_kind::kw_let,
        "token before block comment should still lex correctly");
    test_lexer::assert_true(second_after_comment.kind == token_kind::identifier,
        "token after block comment should still lex correctly");
    test_lexer::assert_true(std::string{sources.slice(second_after_comment.span)} == "value",
        "token span after block comment should slice original source text");
    test_lexer::assert_true(has_flag(second_after_comment.flags, token_flags::leading_space),
        "token after block comment should still observe synthesized separating whitespace");

    diagnostics.clear();
    test_lexer::assert_true(diagnostics.empty(), "api behavior test should not emit diagnostics");
    return 0;
}
